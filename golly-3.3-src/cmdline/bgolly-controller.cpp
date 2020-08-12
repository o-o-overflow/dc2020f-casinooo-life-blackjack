// This file is part of Golly.
// See docs/License.html for the copyright notice.

#include "qlifealgo.h"
#include "hlifealgo.h"
#include "generationsalgo.h"
#include "ltlalgo.h"
#include "jvnalgo.h"
#include "ruleloaderalgo.h"
#include "readpattern.h"
#include "util.h"
#include "viewport.h"
#include "liferender.h"
#include "writepattern.h"
#include <stdlib.h>
#include <iostream>
#include <cstdio>
#include <string.h>
#include <cstdlib>
#include  <sys/shm.h>
#include <bitset>
#include "bgolly-blackjack.h"
#include <chrono>
#include <sys/types.h>
#include <sys/stat.h>
#include <thread>         // std::this_thread::sleep_for
#include <chrono>         // std::chrono::seconds
#include <sys/types.h>
#include <unistd.h>

using namespace std ;

uint16_t *network_send_to_dealer, *network_recv_from_dealer, *zombie_zone;
chrono::steady_clock::time_point last_send_time, last_recv_time;
int cpuid = 0;
int crc_msg_shown_cnt = 0, zero_msg = 0;

double start ;
int maxtime = 0 ;
double timestamp() {
   double now = gollySecondCount() ;
   double r = now - start ;
   if (start == 0)
      start = now ;
   else if (maxtime && r > maxtime)
      exit(0) ;   
   return r ;
}

viewport viewport(1000, 1000) ;
lifealgo *imp = 0 ;

/*
 *   This is a "renderer" that is just stubs, for performance testing.
 */
class nullrender : public liferender {
public:
   nullrender() {}
   virtual ~nullrender() {}
   virtual void pixblit(int, int, int, int, unsigned char*, int) {}
   virtual void getcolors(unsigned char** r, unsigned char** g, unsigned char** b,
                          unsigned char* dead_alpha, unsigned char* live_alpha) {
      static unsigned char dummy[256];
      *r = *g = *b = dummy;
      *dead_alpha = *live_alpha = 255;
   }
} ;
nullrender renderer ;

// the RuleLoader algo looks for .rule files in the user_rules directory
// then in the supplied_rules directory
char* user_rules = (char *)"";              // can be changed by -s or --search
char* supplied_rules = (char *)"Rules/";

int benchmark ; // show timing?
/*
 *   This lifeerrors is used to check rendering during a progress dialog.
 */
class progerrors : public lifeerrors {
public:
   progerrors() {}
   virtual void fatal(const char *s) { cout << "Fatal error: " << s << endl ; exit(10) ; }
   virtual void warning(const char *s) { cout << "Warning: " << s << endl ; }
   virtual void status(const char *s) { 
      if (benchmark)
         cout << timestamp() << " " << s << endl ;
      else {
         timestamp() ;
         cout << s << endl ;
      }
   }
   virtual void beginprogress(const char *s) { abortprogress(0, s) ; }
   virtual bool abortprogress(double, const char *) ;
   virtual void endprogress() { abortprogress(1, "") ; }
   virtual const char* getuserrules() { return user_rules ; }
   virtual const char* getrulesdir() { return supplied_rules ; }
} ;
progerrors progerrors_instance ;

bool progerrors::abortprogress(double, const char *) {
   imp->draw(viewport, renderer) ;
   return 0 ;
}

/*
 *   This is our standard lifeerrors.
 */
class stderrors : public lifeerrors {
public:
   stderrors() {}
   virtual void fatal(const char *s) { cout << "Fatal error: " << s << endl ; exit(10) ; }
   virtual void warning(const char *s) { cout << "Warning: " << s << endl ; }
   virtual void status(const char *s) {
      if (benchmark)
         cout << timestamp() << " " << s << endl ;
      else {
         timestamp() ;
         cout << s << endl ;
      }
   }
   virtual void beginprogress(const char *) {}
   virtual bool abortprogress(double, const char *) { return 0 ; }
   virtual void endprogress() {}
   virtual const char* getuserrules() { return user_rules ; }
   virtual const char* getrulesdir() { return supplied_rules ; }
} ;
stderrors stderrors_instance ;

char *filename ;
struct options {
  const char *shortopt ;
  const char *longopt ;
  const char *desc ;
  char opttype ;
  void *data ;
} ;
bigint maxgen = -1, inc = 0 ;
int maxmem = 256 ;
int hyperxxx ;   // renamed hyper to avoid conflict with windows.h
int render, autofit, quiet, popcount, progress ;
int hashlife ;
char *algoName = 0 ;
int verbose ;
int timeline ;
int stepthresh, stepfactor ;
char *liferule = 0 ;
char *outfilename = 0 ;
char *renderscale = (char *)"1" ;
char *testscript = 0 ;
int outputgzip, outputismc ;
int numberoffset ; // where to insert file name numbers
int resetSharedArg =0;
options options[] = {
  { "-m", "--generation", "How far to run", 'I', &maxgen },
  { "-i", "--stepsize", "Step size", 'I', &inc },
  { "-M", "--maxmemory", "Max memory to use in megabytes", 'i', &maxmem },
  { "-T", "--maxtime", "Max duration", 'i', &maxtime },
  { "-b", "--benchmark", "Show timestamps", 'b', &benchmark },
  { "-2", "--exponential", "Use exponentially increasing steps", 'b', &hyperxxx },
  { "-q", "--quiet", "Don't show population; twice, don't show anything", 'b', &quiet },
  { "-r", "--rule", "Life rule to use", 's', &liferule },
  { "-s", "--search", "Search directory for .rule files", 's', &user_rules },
  { "-h", "--hashlife", "Use Hashlife algorithm", 'b', &hashlife },
  { "-a", "--algorithm", "Select algorithm by name", 's', &algoName },
  { "-o", "--output", "Output file (*.rle, *.mc, *.rle.gz, *.mc.gz)", 's',
                                                               &outfilename },
  { "-R", "--reset", "Reset CPUs specific network shared memory", 'b', &resetSharedArg },
  { "-v", "--verbose", "Verbose", 'b', &verbose },
  { "-t", "--timeline", "Use timeline", 'b', &timeline },
  { "",   "--render", "Render (benchmarking)", 'b', &render },
  { "",   "--progress", "Render during progress dialog (debugging)", 'b', &progress },
  { "",   "--popcount", "Popcount (benchmarking)", 'b', &popcount },
  { "",   "--scale", "Rendering scale", 's', &renderscale },
//{ "",   "--stepthreshold", "Stepsize >= gencount/this (default 1)",
//                                                          'i', &stepthresh },
//{ "",   "--stepfactor", "How much to scale step by (default 2)",
//                                                        'i', &stepfactor },
  { "",   "--autofit", "Autofit before each render", 'b', &autofit },
  { "",   "--exec", "Run testing script", 's', &testscript },
  { 0, 0, 0, 0, 0 }
} ;

int endswith(const char *s, const char *suff) {
   int off = (int)(strlen(s) - strlen(suff)) ;
   if (off <= 0)
      return 0 ;
   s += off ;
   while (*s)
      if (tolower(*s++) != tolower(*suff++))
         return 0 ;
   numberoffset = off ;
   return 1 ;
}

void usage(const char *s) {
  fprintf(stderr, "Usage:  bgolly [options] patternfile\n") ;
  for (int i=0; options[i].shortopt; i++)
    fprintf(stderr, "%3s %-15s %s\n", options[i].shortopt, options[i].longopt,
            options[i].desc) ;
  if (s)
    lifefatal(s) ;
  exit(0) ;
}

#define STRINGIFY(ARG) STR2(ARG)
#define STR2(ARG) #ARG
#define MAXRLE 1000000000

void writepat(int fc) {
   char *thisfilename = outfilename ;
   char tmpfilename[256] ;
   if (fc >= 0) {
      strcpy(tmpfilename, outfilename) ;
      char *p = tmpfilename + numberoffset ;
      *p++ = '-' ;
      sprintf(p, "%d", fc) ;
      p += strlen(p) ;
      strcpy(p, outfilename + numberoffset) ;
      thisfilename = tmpfilename ;
   }
   //cerr << "\t\t\t\t\tPlayer Write: (->" << thisfilename << endl << flush ;
   bigint t, l, b, r ;
   imp->findedges(&t, &l, &b, &r) ;
   if (!outputismc && (t < -MAXRLE || l < -MAXRLE || b > MAXRLE || r > MAXRLE))
      lifefatal("Pattern too large to write in RLE format") ;
   const char *err = writepattern(thisfilename, *imp,
                                  outputismc ? MC_format : RLE_format,
                                  outputgzip ? gzip_compression : no_compression,
                                  t.toint(), l.toint(), b.toint(), r.toint()) ;
   if (err != 0)
      lifewarning(err) ;
   //cerr << ")" << endl << flush ;
}

const int MAXCMDLENGTH = 2048 ;
struct cmdbase {
   cmdbase(const char *cmdarg, const char *argsarg) {
      verb = cmdarg ;
      args = argsarg ;
      next = list ;
      list = this ;
   }
   const char *verb ;
   const char *args ;
   int iargs[4] ;
   char *sarg ;
   bigint barg ;
   virtual void doit() {}
   // for convenience, we put the generic loop here that takes a
   // 4x bounding box and runs getnext on all y values until
   // they are done.  Input is assumed to be a bounding box in the
   // form minx miny maxx maxy
   void runnextloop() {
      int minx = iargs[0] ;
      int miny = iargs[1] ;
      int maxx = iargs[2] ;
      int maxy = iargs[3] ;
      int v ;
      for (int y=miny; y<=maxy; y++) {
         for (int x=minx; x<=maxx; x++) {
            int dx = imp->nextcell(x, y, v) ;
            if (dx < 0)
               break ;
            if (x > 0 && (x + dx) < 0)
               break ;
            x += dx ;
            if (x > maxx)
               break ;
            nextloopinner(x, y) ;
         }
      }
   }
   virtual void nextloopinner(int, int) {}
   int parseargs(const char *cmdargs) {
      int iargn = 0 ;
      char sbuf[MAXCMDLENGTH+2] ;
      for (const char *rargs = args; *rargs; rargs++) {
         while (*cmdargs && *cmdargs <= ' ')
            cmdargs++ ;
         if (*cmdargs == 0) {
            lifewarning("Missing needed argument") ;
            return 0 ;
         }
         switch (*rargs) {
         case 'i':
           if (sscanf(cmdargs, "%d", iargs+iargn) != 1) {
             lifewarning("Missing needed integer argument") ;
             return 0 ;
           }
           iargn++ ;
           break ;
         case 'b':
           {
              int i = 0 ;
              for (i=0; cmdargs[i] > ' '; i++)
                 sbuf[i] = cmdargs[i] ;
              sbuf[i] = 0 ;
              barg = bigint(sbuf) ;
           }
           break ;
         case 's':
           if (sscanf(cmdargs, "%s", sbuf) != 1) {
             lifewarning("Missing needed string argument") ;
             return 0 ;
           }
           sarg = strdup(sbuf) ;
           break ;
         default:
           lifefatal("Internal error in parseargs") ;
         }
         while (*cmdargs && *cmdargs > ' ')
           cmdargs++ ;
      }
      return 1 ;
   }
   static void docmd(const char *cmdline) {
      for (cmdbase *cmd=list; cmd; cmd = cmd->next)
         if (strncmp(cmdline, cmd->verb, strlen(cmd->verb)) == 0 &&
             cmdline[strlen(cmd->verb)] <= ' ') {
            if (cmd->parseargs(cmdline+strlen(cmd->verb))) {
               cmd->doit() ;
            }
            return ;
         }
      lifewarning("Didn't understand command") ;
   }
   cmdbase *next ;
   virtual ~cmdbase() {}
   static cmdbase *list ;
} ;

cmdbase *cmdbase::list = 0 ;

struct loadcmd : public cmdbase {
   loadcmd() : cmdbase("load", "s") {}
   virtual void doit() {
     const char *err = readpattern(sarg, *imp) ;
     if (err != 0)
       lifewarning(err) ;
   }
} load_inst ;
struct stepcmd : public cmdbase {
   stepcmd() : cmdbase("step", "b") {}
   virtual void doit() {
      if (imp->unbounded && (imp->gridwd > 0 || imp->gridht > 0)) {
         // bounded grid, so must step by 1
         imp->setIncrement(1) ;
         if (!imp->CreateBorderCells()) exit(10) ;
         imp->step() ;
         if (!imp->DeleteBorderCells()) exit(10) ;
      } else {
         imp->setIncrement(barg) ;
         imp->step() ;
      }
      if (timeline) imp->extendtimeline() ;
      cout << imp->getGeneration().tostring() << ": " ;
      cout << imp->getPopulation().tostring() << endl ;
   }
} step_inst ;
struct showcmd : public cmdbase {
   showcmd() : cmdbase("show", "") {}
   virtual void doit() {
      cout << imp->getGeneration().tostring() << ": " ;
      cout << imp->getPopulation().tostring() << endl ;
   }
} show_inst ;
struct quitcmd : public cmdbase {
   quitcmd() : cmdbase("quit", "") {}
   virtual void doit() {
      cout << "Buh-bye!" << endl ;
      exit(10) ;
   }
} quit_inst ;
struct setcmd : public cmdbase {
   setcmd() : cmdbase("set", "ii") {}
   virtual void doit() {
      imp->setcell(iargs[0], iargs[1], 1) ;
   }
} set_inst ;
struct unsetcmd : public cmdbase {
   unsetcmd() : cmdbase("unset", "ii") {}
   virtual void doit() {
      imp->setcell(iargs[0], iargs[1], 0) ;
   }
} unset_inst ;
struct helpcmd : public cmdbase {
   helpcmd() : cmdbase("help", "") {}
   virtual void doit() {
      for (cmdbase *cmd=list; cmd; cmd = cmd->next)
         cout << cmd->verb << " " << cmd->args << endl ;
   }
} help_inst ;
struct getcmd : public cmdbase {
   getcmd() : cmdbase("get", "ii") {}
   virtual void doit() {
     cout << "At " << iargs[0] << "," << iargs[1] << " -> " <<
        imp->getcell(iargs[0], iargs[1]) << endl ;
   }
} get_inst ;
struct getnextcmd : public cmdbase {
   getnextcmd() : cmdbase("getnext", "ii") {}
   virtual void doit() {
     int v ;
     cout << "At " << iargs[0] << "," << iargs[1] << " next is " <<
        imp->nextcell(iargs[0], iargs[1], v) << endl ;
   }
} getnext_inst ;
vector<pair<int, int> > cutbuf ;
struct copycmd : public cmdbase {
   copycmd() : cmdbase("copy", "iiii") {}
   virtual void nextloopinner(int x, int y) {
      cutbuf.push_back(make_pair(x-iargs[0], y-iargs[1])) ;
   }
   virtual void doit() {
      cutbuf.clear() ;
      runnextloop() ;
      cout << cutbuf.size() << " pixels copied." << endl ;
   }
} copy_inst ;
struct cutcmd : public cmdbase {
   cutcmd() : cmdbase("cut", "iiii") {}
   virtual void nextloopinner(int x, int y) {
      cutbuf.push_back(make_pair(x-iargs[0], y-iargs[1])) ;
      imp->setcell(x, y, 0) ;
   }
   virtual void doit() {
      cutbuf.clear() ;
      runnextloop() ;
      cout << cutbuf.size() << " pixels cut." << endl ;
   }
} cut_inst ;
// this paste only sets cells, never clears cells
struct pastecmd : public cmdbase {
   pastecmd() : cmdbase("paste", "ii") {}
   virtual void doit() {
      for (unsigned int i=0; i<cutbuf.size(); i++)
         imp->setcell(cutbuf[i].first, cutbuf[i].second, 1) ;
      cout << cutbuf.size() << " pixels pasted." << endl ;
   }
} paste_inst ;
struct showcutcmd : public cmdbase {
   showcutcmd() : cmdbase("showcut", "") {}
   virtual void doit() {
      for (unsigned int i=0; i<cutbuf.size(); i++)
         cout << cutbuf[i].first << " " << cutbuf[i].second << endl ;
   }
} showcut_inst ;

lifealgo *createUniverse() {
   if (algoName == 0) {
     if (hashlife)
       algoName = (char *)"HashLife" ;
     else
       algoName = (char *)"QuickLife" ;
   } else if (strcmp(algoName, "RuleTable") == 0 ||
              strcmp(algoName, "RuleTree") == 0) {
       // RuleTable and RuleTree algos have been replaced by RuleLoader
       algoName = (char *)"RuleLoader" ;
   }
   staticAlgoInfo *ai = staticAlgoInfo::byName(algoName) ;
   if (ai == 0) {
      cout << algoName << endl ; //!!!
      lifefatal("No such algorithm") ;
   }
   lifealgo *imp = (ai->creator)() ;
   if (imp == 0)
      lifefatal("Could not create universe") ;
   imp->setMaxMemory(maxmem) ;
   return imp ;
}

struct newcmd : public cmdbase {
   newcmd() : cmdbase("new", "") {}
   virtual void doit() {
     if (imp != 0)
        delete imp ;
     imp = createUniverse() ;
   }
} new_inst ;
struct sethashingcmd : public cmdbase {
   sethashingcmd() : cmdbase("sethashing", "i") {}
   virtual void doit() {
      hashlife = iargs[0] ;
   }
} sethashing_inst ;
struct setmaxmemcmd : public cmdbase {
   setmaxmemcmd() : cmdbase("setmaxmem", "i") {}
   virtual void doit() {
      maxmem = iargs[0] ;
   }
} setmaxmem_inst ;
struct setalgocmd : public cmdbase {
   setalgocmd() : cmdbase("setalgo", "s") {}
   virtual void doit() {
      algoName = sarg ;
   }
} setalgocmd_inst ;
struct edgescmd : public cmdbase {
   edgescmd() : cmdbase("edges", "") {}
   virtual void doit() {
      bigint t, l, b, r ;
      imp->findedges(&t, &l, &b, &r) ;
      cout << "Bounding box " << l.tostring() ;
      cout << " " << t.tostring() ;
      cout << " .. " << r.tostring() ;
      cout << " " << b.tostring() << endl ;
   }
} edges_inst ;

void runtestscript(const char *testscript) {
   FILE *cmdfile = 0 ;
   if (strcmp(testscript, "-") != 0)
      cmdfile = fopen(testscript, "r") ;
   else
      cmdfile = stdin ;
   char cmdline[MAXCMDLENGTH + 10] ;
   if (cmdfile == 0)
      lifefatal("Cannot open testscript") ;
   for (;;) {
     cerr << flush ;
     if (cmdfile == stdin)
       cout << "bgolly> " << flush ;
     else
       cout << flush ;
     if (fgets(cmdline, MAXCMDLENGTH, cmdfile) == 0)
        break ;
     cmdbase::docmd(cmdline) ;
   }
   exit(0) ;
}
uint16_t getValue(int mempos){
    string bitstr;

    for (int ypos=15; ypos >= 0; ypos--){

        if (imp->getcell(MEMSTART_X+mempos*22, MEMSTART_PLAYER_Y+ypos*22) == 7){
            bitstr += '1';
        } else {
            bitstr += '0';
        }

    }
    //cout << "\t\t\tbitstr: " <<  bitstr << endl;
    unsigned long n = std::bitset<16>(bitstr).to_ulong();
    return (uint16_t) n;
}
//void setMemory(uint16_t tmp, int mempos){
//    int ypos = 15;
//    int bitval =7;
//    string bitstr = bitset<16>(tmp).to_string();
//
//    for(char& c : bitstr) {
//
//        if (c == '0'){
//            bitval = 6;
//        } else {
//            bitval = 7;
//        }
//        imp->setcell(MEMSTART_X+mempos*22, MEMSTART_Y+ypos*22, bitval);
//        imp->setcell(MEMSTART_X+mempos*22, MEMSTART_Y+ypos*22+1, bitval);
//        ypos--;
//    }
//}

void recvMsgFromDealer(){
    int xpos = 15;
    int bitval =7;
    uint16_t tmp = network_recv_from_dealer[cpuid];
    string bitstr = bitset<16>(tmp).to_string();
    int NET_MSG_X_START = 3982;
    int NET_MSG_Y_START = 1051;

    if (tmp != 0){
        for(char& c : bitstr) {
            if (c == '0'){
                bitval = 6;
            } else {
                bitval = 7;
            }
            imp->setcell(NET_MSG_X_START+xpos*11, NET_MSG_Y_START, bitval);
            imp->setcell(NET_MSG_X_START+xpos*11+1, NET_MSG_Y_START, bitval);
            xpos--;
        }
        writepat(-1) ;
#if DEP_VERS > 0
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        long duration = chrono::duration_cast<chrono::milliseconds>(end - last_send_time).count();
        last_recv_time = std::chrono::steady_clock::now();

        uint16_t mt = tmp >> 13;
        cout << "\t\e[38;5;" << dec << (196 + cpuid * 2) << "m CPU #" << dec << cpuid
             << " Elapsed: " << dec << duration
             << " <-- RECVD \e[38;5;2m(" <<dec << mt << " - " << msgType(false, mt) << ") \e[38;5;" << (196+cpuid*2) << "m"
             << " FULL = " << hex << tmp
             << " value = " << hex << (tmp & 0x1FFF) << " bitstr=" << bitstr << "\e[0m" << endl;
#endif
        network_recv_from_dealer[cpuid] = 0;

    }

}

void sendMsgToDealer(){
    string bitstr;
    int NET_MSG_X_START = 3982;
    int NET_MSG_Y_START = 1053;
//    bool found_a_one =  false;
//    for (int xpos=0; xpos < 16; xpos++){
//        if (imp->getcell(NET_MSG_X_START+xpos*11, NET_MSG_Y_START) == 7){
//            found_a_one = true;
//            break;
//        }
//    }
//
//    if (found_a_one){
//        if (imp->getcell(NET_MSG_X_START, NET_MSG_Y_START) == 6 &&
//            imp->getcell(NET_MSG_X_START+11, NET_MSG_Y_START) == 6 &&
//            imp->getcell(NET_MSG_X_START+22, NET_MSG_Y_START) == 6){
//            string tmp_bitstr;
//            for (int xpos=0; xpos < 16; xpos++){
//
//                if (imp->getcell(NET_MSG_X_START+xpos*11, NET_MSG_Y_START) == 7){
//                    tmp_bitstr += '1';
//                } else {
//                    tmp_bitstr += '0';
//                }
//
//            }
//            if (zero_msg % 20 == 0){
//                unsigned long msg_n = std::bitset<16>(tmp_bitstr).to_ulong();
//
//                cout << "#" << dec << cpuid << " Message type 000 is not sent (0x" << hex << msg_n
//                     <<  "),try again next time " << flush << endl;
//            }
//            zero_msg++;
//
//
//            return;
//
//        } else {
//            std::this_thread::sleep_for(std::chrono::milliseconds(200));
//        }
//
//        // wait just 200ms in case was in the middle of copying a value
//
//    } else {
//        return;  // save all the extra cycles.
//    }
    for (int xpos=0; xpos < 16; xpos++){

        if (imp->getcell(NET_MSG_X_START+xpos*11, NET_MSG_Y_START) == 7){
            bitstr += '1';
        } else {
            bitstr += '0';
        }

    }
    unsigned long n = std::bitset<16>(bitstr).to_ulong();

    if (n > 0){
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        long duration = chrono::duration_cast<chrono::milliseconds>(end - last_recv_time).count();
        last_send_time = std::chrono::steady_clock::now();

        uint16_t tmp = (uint16_t) n;
        uint16_t mt = tmp >> 13;
        uint16_t val = tmp & 0x1FFF;

        if (mt == 0){
            if (zero_msg % 20 == 0){
//                cout << " #" << dec << cpuid <<  " Message type is 0 and value is 0x(" << hex << val
//                 << "), will trying again later." << endl;
            }
            zero_msg++;
            return;
        }

        if ((mt == 1 || mt == 2 || mt == 4 || mt == 6) && val == 0){
            if (zero_msg % 20 == 0){
//                cout << " #" << dec << cpuid <<  " Message type is " <<  dec << mt << " and value is 0x(" << hex << val
//                     << "), will trying again later." << endl;
            }
            zero_msg++;
            return;
        }
        for (int xpos=0; xpos < 16; xpos++){
            imp->setcell(NET_MSG_X_START+xpos*11, NET_MSG_Y_START, 6);
            imp->setcell(NET_MSG_X_START+xpos*11+1, NET_MSG_Y_START, 6);
        }
        zero_msg = 0;
#if DEP_VERS > 0
        cout << "\t\e[38;5;" << dec << (196 + cpuid * 2) << "m CPU #" << dec << cpuid
             << " Elapsed: " << dec << duration
             << "s --> SEND \e[38;5;2m(" <<dec << mt << " - " << msgType(true, mt) << ") \e[38;5;" << (196 + cpuid * 2) << "m"
             << " FULL = " << hex << tmp
             << " value = " << hex << (tmp & 0x1FFF) << " bitstr=" << bitstr << "\e[0m" << endl;
        if (getenv("DEBUG")) {
            if (n >> 13 == 1){
                n = 1 << 13 | (cpuid+1000);
            }
        }
#endif
        network_send_to_dealer[cpuid] = (uint16_t) n;
        writepat(-1) ;
    }

}
void setSeed(){
    // write out seed value
    int SEED_X_START = 4058;
    int SEED_Y_START = 596;
    int xpos = 15;
    int bitval =7;
    uint16_t seedval = cpuid;

    string bitstr = bitset<16>(seedval).to_string();
    for(char& c : bitstr) {
        if (c == '0'){
            bitval = 6;
        } else {
            bitval = 7;
        }
        imp->setcell(SEED_X_START+xpos*11, SEED_Y_START, bitval);
        imp->setcell(SEED_X_START+xpos*11+1, SEED_Y_START, bitval);
        xpos--;
    }

    bitstr = "";
    for (xpos=0; xpos < 16; xpos++){
        if (imp->getcell(SEED_X_START+xpos*11, SEED_Y_START) == 7){
            bitstr += '1';
        } else {
            bitstr += '0';
        }
    }

    cout << "bits=" << bitstr << endl;

}
bool init_game_data(bool resetShared){
    uint64_t net_shm_id = shmget(NETWORK_SHM_KEY , NETWORK_SHARED_SIZE, 0);
    network_send_to_dealer = (uint16_t *) shmat(net_shm_id, NULL, 0);
    cpuid = 1;
    if (getenv("CPUID")){
        cpuid = atoi(getenv("CPUID"));

    }
    cout << "CPUID=" << cpuid << endl;
    // write out seed value

    network_recv_from_dealer = network_send_to_dealer + (NETWORK_PLAYER_OUT_SIZE/sizeof(uint16_t));

    uint64_t shm_id = shmget(ZOMBIE_KEY , ZOMBIE_SIZE, IPC_CREAT | 0666);
    zombie_zone = (uint16_t *) shmat(shm_id, NULL, 0);
    network_recv_from_dealer[cpuid] = 0;
    if (resetShared){
        network_recv_from_dealer[cpuid] = 0;
        network_send_to_dealer[cpuid] = 0;
    }
    return true;
}

time_t file_last_modified(const char *path) {
    struct stat file_stat;
    int err = stat(path, &file_stat);
    if (err != 0) {
        perror(" [file_last modified] stat error");
        exit(errno);
    }

    return file_stat.st_mtime ;
}
size_t file_size(const char *path) {
    struct stat file_stat;
    int err = stat(path, &file_stat);
    if (err != 0) {
        perror(" [file_size] stat failed ");
        exit(errno);
    }

    return file_stat.st_size ;
}

bool checkForReset(){
    uint16_t crc_value =0;
    for(int i =0; i <= 600; i++){
        for(int j =0; j <= 600; j++){
            uint16_t tmp = imp->getcell(i, j);
            crc_value += imp->getcell(i, j);
        }
    }
#if DEP_VERS > 0
    if (crc_value == 0 & crc_msg_shown_cnt < 3){
        cout << "\t\e[38;5;" << dec << (196 + cpuid * 2) << "m CPU #" << dec << cpuid
            << "********* >>>>>> CRC value = " << dec << crc_value << " <<<<<<<<< ********** \e[0m" << endl;
        crc_msg_shown_cnt++;
    }
#endif
    if (crc_value == 0){
        return true;
    }
    return false;

}

#if DEP_VERS > 0

void write_redacted(){
    char *thisfilename = outfilename ;
    char tmpfilename[256] ;
    strcpy(tmpfilename, outfilename) ;
    char *p = tmpfilename + numberoffset ;
    *p++ = '-' ;
    sprintf(p, "%s", "REDACTED") ;
    p += strlen(p) ;
    strcpy(p, outfilename + numberoffset) ;
    thisfilename = tmpfilename ;
    bigint t, l, b, r ;
    imp->findedges(&t, &l, &b, &r) ;
    if (!outputismc && (t < -MAXRLE || l < -MAXRLE || b > MAXRLE || r > MAXRLE))
        lifefatal("Pattern too large to write in RLE format") ;

    for(int x=581;x < 3365;x++){
        for(int y=1; y < 600; y++ ){
            imp->setcell(x, y, 0);
        }
    }

    const char *err = writepattern(thisfilename, *imp,
                                  outputismc ? MC_format : RLE_format,
                                  outputgzip ? gzip_compression : no_compression,
                                  t.toint(), l.toint(), b.toint(), r.toint()) ;

}

#endif

int main(int argc, char *argv[]) {

   last_recv_time = std::chrono::steady_clock::now();
//   cout << "This is bgolly " STRINGIFY(VERSION) " Copyright 2005-2019 The Golly Gang."
//        << endl ;
//   cout << "-" ;
//   for (int i=0; i<argc; i++)
//      cout << " " << argv[i] ;
//   cout << endl << flush ;
   qlifealgo::doInitializeAlgoInfo(staticAlgoInfo::tick()) ;
   hlifealgo::doInitializeAlgoInfo(staticAlgoInfo::tick()) ;
   generationsalgo::doInitializeAlgoInfo(staticAlgoInfo::tick()) ;
   ltlalgo::doInitializeAlgoInfo(staticAlgoInfo::tick()) ;
   jvnalgo::doInitializeAlgoInfo(staticAlgoInfo::tick()) ;
   ruleloaderalgo::doInitializeAlgoInfo(staticAlgoInfo::tick()) ;
   while (argc > 1 && argv[1][0] == '-') {
      argc-- ;
      argv++ ;
      char *opt = argv[0] ;
      int hit = 0 ;
      for (int i=0; options[i].shortopt; i++) {
        if (strcmp(opt, options[i].shortopt) == 0 ||
            strcmp(opt, options[i].longopt) == 0) {
          switch (options[i].opttype) {
case 'i':
             if (argc < 2)
                lifefatal("Bad option argument") ;
             *(int *)options[i].data = atol(argv[1]) ;
             argc-- ;
             argv++ ;
             break ;
case 'I':
             if (argc < 2)
                lifefatal("Bad option argument") ;
             *(bigint *)options[i].data = bigint(argv[1]) ;
             argc-- ;
             argv++ ;
             break ;
case 'b':
             (*(int *)options[i].data) += 1 ;
             break ;
case 's':
             if (argc < 2)
                lifefatal("Bad option argument") ;
             *(char **)options[i].data = argv[1] ;
             argc-- ;
             argv++ ;
             break ;
          }
          hit++ ;
          break ;
        }
      }
      if (!hit)
         usage("Bad option given") ;
   }
   if (argc < 2 && !testscript)
      usage("No pattern argument given") ;
   if (argc > 2)
      usage("Extra stuff after pattern argument") ;
   if (outfilename) {
      if (endswith(outfilename, ".rle")) {
      } else if (endswith(outfilename, ".mc")) {
         outputismc = 1 ;
#ifdef ZLIB
      } else if (endswith(outfilename, ".rle.gz")) {
         outputgzip = 1 ;
      } else if (endswith(outfilename, ".mc.gz")) {
         outputismc = 1 ;
         outputgzip = 1 ;
#endif
      } else {
         lifefatal("Output filename must end with .rle or .mc.") ;
      }
      if (strlen(outfilename) > 200)
         lifefatal("Output filename too long") ;
   }
   if (timeline && hyperxxx)
      lifefatal("Cannot use both timeline and exponentially increasing steps") ;
   imp = createUniverse() ;
   if (progress)
      lifeerrors::seterrorhandler(&progerrors_instance) ;
   else
      lifeerrors::seterrorhandler(&stderrors_instance) ;
   if (verbose) {
      hlifealgo::setVerbose(1) ;
   }
   imp->setMaxMemory(maxmem) ;
   timestamp() ;
   if (testscript) {
      if (argc > 1) {
         filename = argv[1] ;
         const char *err = readpattern(argv[1], *imp) ;
         if (err) lifefatal(err) ;
      }
      runtestscript(testscript) ;
   }
   filename = argv[1] ;
   const char *err = readpattern(argv[1], *imp) ;
   if (err) lifefatal(err) ;
   if (liferule) {
      err = imp->setrule(liferule) ;
      if (err) lifefatal(err) ;
   }



   bool boundedgrid = imp->unbounded && (imp->gridwd > 0 || imp->gridht > 0) ;
   if (boundedgrid) {
      if (hyperxxx || inc > 1)
         lifewarning("Step size must be 1 for a bounded grid") ;
      hyperxxx = 0 ;
      inc = 1 ;     // only step by 1
   }
   if (inc != 0)
      imp->setIncrement(inc) ;
   if (timeline) {
      int lowbit = inc.lowbitset() ;
      bigint t = 1 ;
      for (int i=0; i<lowbit; i++)
         t.mul_smallint(2) ;
      if (t != inc)
         lifefatal("Bad increment for timeline") ;
      imp->startrecording(2, lowbit) ;
   }

    int child_pid = fork();

    if (child_pid < 0) exit(4);

    if (!child_pid) {  // child_pid == 0, i.e., in child
#if DEP_VERS > 0
        write_redacted();
        cout << "wrote out redacted" << endl;
#endif
        exit(0);
    }


   int fc = 0 ;

   /**
   * Start of main event loop
   */
   bool resetShared = resetSharedArg > 0;
   if (!init_game_data(resetShared)){
       return 99;
   }

   string waiting_fn = "/tmp/inputs/laas-bj-teams-controller-" + to_string(cpuid) + ".mc";
   cout << waiting_fn << endl;
   time_t pattern_last_modified = file_last_modified(waiting_fn.c_str());
   size_t pattern_last_size = file_size(waiting_fn.c_str());

   int act = 8;
   setSeed();

   for (;;) {

      sendMsgToDealer();
      recvMsgFromDealer();

//      uint16_t t1 = (uint16_t) (network_send_to_dealer[cpuid] & 0xE000);
//      uint16_t t2 = (uint16_t) (network_send_to_dealer[cpuid] & 0x1FFF);
//      uint16_t t3 = (uint16_t) (network_recv_from_dealer[cpuid]  & 0xE000);
//      uint16_t t4 = (uint16_t) (network_recv_from_dealer[cpuid]  & 0x1FFF);
//      if (t1 > 0){
//          cout << "\t\tPlayer Sending msg = "<< network_send_to_dealer[cpuid] << " response_type=" << t1 << " payload=" << t2 << "  RECV FROM Dealer: " << t3 << " " << t4 << endl;
//      }


      if (!quiet) {
          if ( getValue(act+6) > 0 || getValue(act+1) > 0) {
              cout << "\t\tSession ID = " << hex << getValue(act+4);
              cout << " Score = " << dec << getValue(act+6)
                   << " <" << hex << getValue(act+7) << " " <<  hex << getValue(act+8)
                   << " " <<  hex << getValue(act+9) << " " <<  hex << getValue(act+10) << ">" << endl;
          } else {
              cout << "\tPlayer: \e[33m" << imp->getGeneration().tostring() << " PC:" << dec << getValue(0)
                   << " ACT:" << getValue(act) <<  " incmsg=" << getValue(act+1)
                   << "\e[0m" << endl ;
          }


     }
//      if (network_send_to_dealer[cpuid] == 0 && t3 == 0x2000){
//          cout << "\t\tGOT RESULTS, deploying a BET of 31 bucks" << endl;
//          network_send_to_dealer[cpuid] = 0x402F;
//      };

      if (benchmark)
         cout << timestamp() << " " ;
      else
         timestamp() ;
      if (quiet < 2) {
         //cout << "memcell" << imp->getcell(3475+26*22, -2) << endl;
         //cout << "getting generation " << imp->getcell(3453, 54)<< endl;
         if (!quiet) {
            const char *s = imp->getPopulation().tostring() ;
            if (benchmark) {
               cout << endl ;
               cout << timestamp() << " pop " << s << endl ;
            } else {
               //cout << "\tPlayer: \e[33m" << imp->getGeneration().tostring() << ": " << s << "\e[0m" << endl ;
            }
         } else{
           //cout << "\tPlayer: \e[33m" << imp->getGeneration().tostring() << "\e[0m" << endl;
         }
      }
      if (popcount)
         imp->getPopulation() ;
      if (autofit)
        imp->fit(viewport, 1) ;
      if (render)
        imp->draw(viewport, renderer) ;
      if (maxgen >= 0 && imp->getGeneration() >= maxgen)
         break ;
      if (!hyperxxx && maxgen > 0 && inc == 0) {
         bigint diff = maxgen ;
         diff -= imp->getGeneration() ;
         int bs = diff.lowbitset() ;
         diff = 1 ;
         diff <<= bs ;
         imp->setIncrement(diff) ;
      }
      if (boundedgrid && !imp->CreateBorderCells()) break ;
      imp->step() ;
      if (boundedgrid && !imp->DeleteBorderCells()) break ;
      if (timeline) imp->extendtimeline() ;
      if (maxgen < 0 && outfilename != 0)
         writepat(fc++) ;
      if (timeline && imp->getframecount() + 2 > MAX_FRAME_COUNT)
         imp->pruneframes() ;
      if (hyperxxx)
         imp->setIncrement(imp->getGeneration()) ;
      int player_action_mem_pos = 8;
      uint16_t actionVal = getValue(player_action_mem_pos);

      time_t current_filetime = file_last_modified(waiting_fn.c_str());
      size_t current_size = file_size(waiting_fn.c_str());

      if (current_filetime >  pattern_last_modified && checkForReset()){
#if DEP_VERS > 0

          cout << endl << "Detected pattern file modification, quiting" << endl;
#endif
          network_send_to_dealer[cpuid] = RELOAD;
          zombie_zone[cpuid] = 1;
          exit(0);
      }

      if (actionVal >= 5){
          cout << "PLAYER's ACTION VALUE IS  = " << actionVal << "... Exiting! " << endl;

          break;
      }


      writepat(-1);

   }
//   if (maxgen >= 0 && outfilename != 0)
//      writepat(-1) ;
   exit(0) ;
}
