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
#include <fstream>
#include <cstdio>
#include <string.h>
#include <cstdlib>
#include <sys/shm.h>
#include <bitset>
#include <queue>
#include <chrono>
#include <thread>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <signal.h>
#include <sys/random.h>
#include <math.h>

#include "bgolly-blackjack.h"

#define PLAYER_SHARED_SIZE 0x1000
#define PLAYER_SHM_KEY 0x10C001
#define SESSION_SHM_KEY 0x10D002
#define SESSION_SHARED_SIZE 0x1000
#define CARD_SHM_KEY 0x10E003
#define CARD_SHARED_SIZE 0x1000
#define CHILD_SHM_KEY 0x10F004
#define CHILD_SHARED_SIZE 0x1000
#define SERVER_ID 0x1337

const int gol_action = 11;
const int gol_incoming_type        = gol_action + 1;
const int gol_dlr_result           = gol_action + 3;
const int gol_dlr_cards            = gol_action + 4;

const int gol_player_cards         = gol_action + 5;
// 6
//const int gol_player_future_cards  = gol_action + 7;
const int gol_player_result        = gol_action + 6;
const int gol_new_card             = gol_action + 7;
// acecnt +8
const int gol_player_bank          = gol_action + 9;
const int gol_player_bet           = gol_action + 10;
const int gol_netmsg               = gol_action + 11;
const int gol_outmsg               = gol_action + 12;
const int gol_session_id           = gol_action + 13;
const int gol_player_id            = gol_action + 15;

const int FORCED_TIMEOUT = 60;
const int EARLY_TIMEOUT = 20;

struct NetworkMsg {
    uint16_t cpuid;
    uint16_t msg;
    NetworkMsg(uint16_t cpuid, uint16_t msg)
        : cpuid(cpuid), msg(msg)
    { /*empty*/ }
};

struct Player {
    uint16_t cards =0;
    uint16_t result =0;
    uint16_t bank = 100;
    uint16_t bet = 10 ;
    uint16_t session_id = 1337;
    uint16_t lastHandResult = 0;
    char cardTrack[10];
    bool done_with_hand = false;
    bool started_hand = false;
    bool zombie=false;
    int id =0;
    int initalized=31337;
};

using namespace std ;
string round_id = "";
uint16_t dealer_cards = 0;
uint16_t dealer_result = 0;
string dealerTrackCards = "";
uint16_t *session_data, *card_data, *zombie_zone;
int *children_shared, *cpu_track_shared;

int shuffle_y_adj =0;
chrono::steady_clock::time_point child_start_time, shuffle_start_time;

uint16_t *network_recv_from, *network_send_to;
queue<uint16_t> currentcards;
queue<queue<uint16_t>> futurecards;
unordered_set<int> childPidSet;

struct Player *player_data;

queue<NetworkMsg> netMsgs;

unordered_map<int, int> cpu_to_player, player_to_cpu;

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
int shuffler = 0, resetshared=0;
int stepthresh, stepfactor ;
char *liferule = 0 ;
char *outfilename = 0 ;
char *renderscale = (char *)"1" ;
char *testscript = 0 ;
int outputgzip, outputismc ;
int numberoffset ; // where to insert file name numbers
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
  { "-v", "--verbose", "Verbose", 'b', &verbose },
  { "-t", "--timeline", "Use timeline", 'b', &timeline },
  { "-S", "--shuffler", "Process Shuffler ", 'b', &shuffler },
  { "-R", "--reset", "Reset shared memory", 'b', &resetshared },
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
   //cerr << "\t\t\t\t\tWorker: (->" << thisfilename << endl << flush ;
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

void signal_handler(int sig) {
    signal(sig, signal_handler);
    exit(123);
}

struct Player *initPlayer(int playerPos, uint16_t player_id){
    struct Player *p = player_data + playerPos;
    p->cards = 0;
    p->result = 0;
    p->bank = 200;
    p->bet = 20;
    p->session_id = session_data[playerPos];
    p->cardTrack[0] = '\x00';
    p->done_with_hand = false;
    p->started_hand = false;
    p->id = player_id;
    return p;
}
struct Player *getPlayerByPId(int playerId){
    struct Player p;
    for (int i=0; i < 16; i++){
        p = player_data[i];
        if (playerId == p.id){
            return player_data + i;
        }
    }
    return NULL;

}
struct Player *getPlayerByCookie(int cookieId){
    struct Player p;
    for (int i=0; i < 16; i++){
        p = player_data[i];
        if (cookieId == p.session_id){
            return player_data + i;
        }
    }
    return NULL;

}

uint16_t getValue(int mempos){
    string bitstr;

    for (int ypos=15; ypos >= 0; ypos--){

        if (imp->getcell(MEMSTART_X+mempos*22, MEMSTART_Y+shuffle_y_adj+ypos*22) == 7){
            bitstr += '1';
        } else {
            bitstr += '0';
        }

    }
//    cout << "bitstr: " <<  bitstr << endl;
    unsigned long n = std::bitset<16>(bitstr).to_ulong();
    return (uint16_t) n;
}

void setMemory(uint16_t tmp, int mempos){
    int ypos = 15;
    int bitval =7;
    string bitstr = bitset<16>(tmp).to_string();

    for(char& c : bitstr) {

        if (c == '0'){
            bitval = 6;
        } else {
            bitval = 7;
        }
        imp->setcell(MEMSTART_X+mempos*22, MEMSTART_Y+shuffle_y_adj+ypos*22, bitval);
        imp->setcell(MEMSTART_X+mempos*22, MEMSTART_Y+shuffle_y_adj+ypos*22+1, bitval);
        ypos--;
    }
}

bool init_game_data(bool shuffling, bool resetShared){

    uint64_t shm_id = shmget(PLAYER_SHM_KEY , PLAYER_SHARED_SIZE, IPC_CREAT | 0666);
    if (shm_id < 0){
       printf("error shmget failed!!!\n");
       return false;
    }
    player_data = (Player *) shmat(shm_id, NULL, 0);

    // card init
    shm_id = shmget(SESSION_SHM_KEY , SESSION_SHARED_SIZE, IPC_CREAT | 0666);
    session_data = (uint16_t *) shmat(shm_id, NULL, 0);
    shm_id = shmget(CARD_SHM_KEY , CARD_SHARED_SIZE, IPC_CREAT | 0666);
    card_data = (uint16_t *) shmat(shm_id, NULL, 0);

    uint64_t net_shm_id = shmget(NETWORK_SHM_KEY , NETWORK_SHARED_SIZE, IPC_CREAT | 0666);
    network_recv_from = (uint16_t *) shmat(net_shm_id, NULL, 0);
    network_send_to = network_recv_from + (NETWORK_PLAYER_OUT_SIZE/sizeof(uint16_t));

    shm_id = shmget(CHILD_SHM_KEY , CHILD_SHARED_SIZE, IPC_CREAT | 0666);
    children_shared = (int *) shmat(shm_id, NULL, 0);

    cpu_track_shared = children_shared + sizeof(int *)*50;

    shm_id = shmget(ZOMBIE_KEY , ZOMBIE_SIZE, IPC_CREAT | 0666);
    zombie_zone = (uint16_t *) shmat(shm_id, NULL, 0);

    //cout << "Player address = " << shm_id << " " << static_cast<void*>(player_data) << std::endl;
    //cout << "Network address = " << net_shm_id << " " << static_cast<void*>(network_recv_from) << std::endl;
    if (shuffling){
        memset(card_data, 0, CARD_SHARED_SIZE);
        memset(session_data, 0, SESSION_SHARED_SIZE);

    } else {
        if (player_data->initalized == 31337 && !resetShared){
            cout << "\e[36m[*] Restart detected, not initalizing shared memory\e[0m" << endl;
        } else {
            memset(player_data, 0, PLAYER_SHARED_SIZE);
            memset(network_recv_from, 0, NETWORK_SHARED_SIZE);
            memset(children_shared, 0, CHILD_SHARED_SIZE);
            memset(cpu_track_shared, 0, 100);
            memset(zombie_zone, 0, ZOMBIE_SIZE);
        }

    }

    return true;
}

uint16_t adjusted_card(uint16_t cardin){
    if (cardin > 10){
        return 10;
    }
    return cardin;
}

uint16_t build_hand(){
    //cout << "\t\t\t\tPlayer Cards = ";
    uint16_t firstcard = currentcards.front();
    currentcards.pop();

    uint16_t secondcard = currentcards.front();
    currentcards.pop();
    //cout << hex << firstcard << " " << hex << secondcard;
    uint16_t results = adjusted_card(firstcard);
    results += adjusted_card(secondcard);

    //cout << " total: " << dec << results;
    results = firstcard << 12 | secondcard << 8 | results;

    if (firstcard == 1 || secondcard == 1){
        results = results | 0b10000000;
    }
    //cout << " combined  " << hex << results;
    return results;

}

int time_for_new_hand(int seconds, chrono::steady_clock::time_point start_time){

  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

  long duration = chrono::duration_cast<chrono::milliseconds>(end - start_time).count();
  //cout << "dur=" << dec << duration << endl;
  if (duration > (seconds * 1000)){
      return true;
  }
  return false;
}

string convertCard(uint16_t cardval){
    switch (cardval) {
        case 0: return "";
        case 1: return "A";
        case 10: return "T";
        case 11: return "J";
        case 12: return "Q";
        case 13: return "K";
        default:
            return to_string(cardval);

    }
}

void calcAndSaveScores(){

    ofstream jout;
    struct Player *p;
    for(int targetcpu = 1; targetcpu <= 16; targetcpu++) {
        //printf("\t\t\t\tPlayer id: %d, session_id:%d, cards=%x, bank=%d\n", p.id, p.session_id,p.cards, p.bank );
        jout.open("/tmp/round-" + round_id + "/koh_scores/team-" + to_string(targetcpu) + ".dat");
        if (cpu_to_player.find(targetcpu) == cpu_to_player.end()){
            jout << 0 << endl;
        } else {
            int player_id = cpu_to_player[targetcpu];
            p = getPlayerByPId(player_id);
            jout << (p->bank+1) << endl;
        }
        jout.close();
    }

    //cout << "DONE writint out " << endl;

}

void writeOutResults(int handnum){

    ofstream jout;
    string json_fn = "/tmp/round-" + round_id + "/handhist/hand-" + to_string(handnum) + ".json";
    jout.open (json_fn);
    calcAndSaveScores();
    jout << "{\"hand\":[";
    jout << "{"
        << "\"name\":\"" << "Dealer" << "\","
        << "\"cards\":\"" << dealerTrackCards << "\","
        << "\"wager\":\"" << 0 << "\","
        << "\"bank\":\"" << 0 << "\","
        << "\"handResult\":\"" << 0 << "\","
        << "\"handNum\":\"" << handnum << "\","
        << "\"controller\":\"" << "B'jAI" << "\","
        << "\"pid\":\"" << "1337" << "\""
        << "}";
    struct Player p;
    for(int i = 0; i < 16; i++) {

        //printf("\t\t\t\tPlayer id: %d, session_id:%d, cards=%x, bank=%d\n", p.id, p.session_id,p.cards, p.bank );
        p = player_data[i];

        if (p.cards == 0){
            uint16_t playershand = build_hand();
            string tmpstr = "";
            tmpstr += convertCard((playershand >> 8)&0xF);
            tmpstr += convertCard((playershand >> 12)&0xF);
            //cout << "\e[31m" << hex << playershand << " --> " << tmpstr << "\e[0m" << endl;
            strcpy(p.cardTrack,tmpstr.c_str());
        }
        string controller = "";
        for (int cpu=0; cpu<=16;cpu++){
            if (cpu_to_player.find(cpu) != cpu_to_player.end()){
                 int temp_player_id = cpu_to_player[cpu];
                 if (temp_player_id == p.id){
                     controller += "" + to_string(cpu) + " ";
                 }
            }
        }
        if (p.bet == 0){
             jout << "," << endl << "{"
                 << "\"name\":\"" << "Player " << (i+1) << "\","
                 << "\"cards\":\"" << "" << "\","
                 << "\"wager\":\"" << 0 << "\","
                 << "\"bank\":\"" << p.bank << "\","
                 << "\"handResult\":\"" << p.lastHandResult << "\","
                 << "\"handNum\":\"" << handnum << "\","
                 << "\"controller\":\"" << controller << "\","
                 << "\"pid\":\"" << p.id << "\""
                 << "}";
        } else {
            jout << "," << endl << "{"
                 << "\"name\":\"" << "Player " << (i+1) << "\","
                 << "\"cards\":\"" << string(p.cardTrack) << "\","
                 << "\"wager\":\"" << p.bet << "\","
                 << "\"bank\":\"" << p.bank << "\","
                 << "\"handResult\":\"" << p.lastHandResult << "\","
                 << "\"handNum\":\"" << handnum << "\","
                 << "\"controller\":\"" << controller << "\","
                 << "\"pid\":\"" << p.id << "\""
                 << "}";
        }

    }
    jout << endl << "]}" << endl;
    jout.close();
    cout << "\t\t\t\tHand history is saved for hand #" << to_string(handnum) << endl;
}

void initPcapFile(){
    ofstream wf("/tmp/round-" + round_id + "/gol.pcap", ios::out | ios::binary);
    wf.write("\xd4\xc3\xb2\xa1\x02\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00\xdc\x05\x00\x00\x01\x00\x00\x00",24);
    wf.close();
}
void recordNetworkComm(uint32_t dst, uint32_t src, uint32_t data){
   auto now = std::chrono::system_clock::now();
   auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
   auto epoch = now_ms.time_since_epoch();
   auto value = std::chrono::duration_cast<std::chrono::milliseconds>(epoch);
   double duration = ((double) value.count()) / 1000;
   double nanos = fmod(duration,1);
   int duration_int = value.count() / 1000;
   int nanos_int = nanos * 1000;

   ofstream wf("/tmp/round-" + round_id + "/gol.pcap", ios::app | ios::binary);

   wf.write((char *) &duration_int, 4);
   wf.write((char *) &nanos_int, 2);
   wf.write("\x07\x00",2);
   wf.write("\x10\x00\x00\x00",4 );
   wf.write("\x10\x00\x00\x00",4 );

   long dst_out = __builtin_bswap32(dst);

   long src_out = __builtin_bswap32(src);
   wf.write("\x00\x00", 2);
   wf.write((char *) &dst_out, 4);
   wf.write("\x00\x00", 2);
   wf.write((char *) &src_out, 4);

   wf.write("\x08\x00", 2);

   uint8_t first = data >> 8;
   uint8_t second = data & 0xFF;
   wf.write((char *) &first, 1);
   wf.write((char *) &second, 1);
   wf.close();
}

void sendMessage(int cpuid, uint16_t msg_type, uint16_t msg ){
    network_send_to[cpuid] = msg_type | msg;
    recordNetworkComm(cpuid, SERVER_ID, network_send_to[cpuid]);
}

int wait_for_msg(){
    uint16_t msg = 0;
    int loopcnt =0;
    pid_t child_pid = -1;
    int handnum = 1;

    bool start_hand = true;
    chrono::steady_clock::time_point init_time = std::chrono::steady_clock::now();
    chrono::steady_clock::time_point start_time;

    int unusedPlayerSlots = 0;
    // main loop for worker (not shuffler)
    while (true){

        loopcnt++;
        if (loopcnt % 50 == 0){
            calcAndSaveScores();
        }

        for (uint16_t i =0; i < 17; i++){
            msg = network_recv_from[i];
            if (msg != 0){
                recordNetworkComm(SERVER_ID, i, msg);

                NetworkMsg nm = {i, msg};
                netMsgs.push(nm);
//                std::this_thread::sleep_for(std::chrono::milliseconds(5*1000));
                uint16_t msg_type = msg >> 13;
#if DEP_VERS > 0
                cout << "\t\t\t\t\e[33mAdding message to queue "
                     << " for cpuid " << dec << i
                     << "(" << msg_type << "-" << msgType(true, msg_type) << ")"
                     << " msg= 0x" << hex << (msg) << " \e[0m"
                     << " value= 0x" << hex << ( msg & 0x1FFF)
                     << endl;
#else
                cout << "\t\t\t\t\e[33mAdding message to queue " << hex << msg_type << "\e[0m" << endl;

#endif
                network_recv_from[i] = 0;
            }
        }
        if (start_hand){
            start_hand = false;
            start_time = chrono::steady_clock::now();
            if (!futurecards.empty()){
                currentcards = futurecards.front();
                futurecards.pop();
            } else {
#if DEP_VERS > 0
                if (handnum > 1){
                   cout << "\t\t\t\t\e[31mFuture cards is empty??? \e[0m" << endl;
                }
#endif
            }

            dealer_cards = 0;
            dealer_result = 0;
        }
        // load cards
        if (card_data[25] != 0 && futurecards.size() < 3){
            queue<uint16_t> cardqueue;
            bool found_empty = false;
            for(int i=0; i < 13;i++){
                if (card_data[i] == 0){
                    found_empty = true;
                }
            }
            if (found_empty){
                continue;
            }
            cout << "\t\t\t\tGen'd cards: [ 104 new cards ";
            for(int i=0; i < 26;i++){
                for (int j=0; j < 4;j++){
                    cardqueue.push(card_data[i] & 0xF);
                    //cout << hex << (card_data[i] & 0xF) << " ";
                    card_data[i] = card_data[i] >> 4;
                }
                card_data[i] = 0;
            }

            if (currentcards.empty()){
                currentcards = cardqueue;
            } else {
                futurecards.push(cardqueue);
            }
            cout << " ] (size= " << futurecards.size() << ")" << endl;
        }

        if (currentcards.empty() ){
            std::this_thread::sleep_for(std::chrono::milliseconds(1*1000));
            cout << "\t\t\t\tNo cards yet, waiting..." << endl;
            continue;
        }

        bool can_early_stop = true;
        for (int i=0; i < 16; i++){
            // if any player has started_hand and is not done, then CANNOT early stop
            if (player_data[i].started_hand && !player_data[i].done_with_hand){
                can_early_stop = false;
            }
        }
        can_early_stop = can_early_stop && time_for_new_hand(EARLY_TIMEOUT, start_time);

        // restart hand
        if ((time_for_new_hand(FORCED_TIMEOUT, start_time) || can_early_stop )){
           writeOutResults(handnum++);

           start_hand = true;
           // clear currentcards queue
           queue<uint16_t> empty;
           swap( currentcards, empty );

           // ADD kill children here.

           for (int i=0; i < 16; i++){
                if (player_data[i].started_hand && !player_data[i].done_with_hand){
                    player_data[i].bank -= player_data[i].bet;
                }
                player_data[i].cards = 0;
                player_data[i].result =0;
                player_data[i].done_with_hand = false;
                player_data[i].started_hand = false;
                player_data[i].cardTrack[0] = '\x00';
           }

           dealer_cards = 0;
           dealer_result = 0;

           chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
           long tots_dur = chrono::duration_cast<chrono::milliseconds>(end - init_time).count();
#if DEP_VERS > 0
           cout << "\t\t\t\t\e[38;5;10m<@@>Starting hand #" << dec << handnum
                << " for Round #" << dec << round_id
                << " round duration = "  << dec <<   (tots_dur/1000) << "s <@@>\e[0m" << endl;
#else
           cout << "\t\t\t\tStarting next hand" << endl;
#endif
           continue;

        }

        // start next hand here
        if (dealer_cards == 0){
            bool dealer_has_ace = false;
            cout << "\t\t\t\t" << "Dealer Cards: Recvd[ ";
            dealerTrackCards = "";
            bool using_ace = false;
            while (dealer_result <= 16 || (dealer_result==17 && using_ace)){
                uint16_t cardval = currentcards.front();
                currentcards.pop();
                dealerTrackCards += convertCard(cardval);
                dealer_result += adjusted_card(cardval);
                if (cardval == 1){
                    dealer_has_ace = true;
                }
                if (dealer_cards == 0){
                    dealer_cards = cardval;
                }
                if (dealer_has_ace) {
                    uint16_t ace_result = dealer_result + 10;
                    if (ace_result >= 17 && ace_result <= 21){
                        dealer_result += 10;
                        using_ace = true;
                    }
                }
                cout << hex << cardval << " ";
            }
            cout << "] " << " Showing: " << hex << dealer_cards;
            if (dealerTrackCards.length() == 2 && dealer_result == 21){
                dealer_result = 255;
                cout << " DEALER BLACKJACK!!!!!!!" << endl;
            } else {
                cout << " Total: " << dec << dealer_result << endl;
            }

        }

        // No messages in message queue, wait and try again.
        if (netMsgs.empty()){
            int i = 0;
            struct Player p = player_data[i++];
            while (p.id != 0){
                //printf("\t\t\t\tPlayer id: %d, session_id:%d, cards=%x, bank=%d\n", p.id, p.session_id,p.cards, p.bank );
                p = player_data[i++];
            }
            //printf("\t Waiting.....\n");
            this_thread::sleep_for(std::chrono::milliseconds(1*200));
            continue;
        }

        // Message processing starts
        NetworkMsg nm = netMsgs.front();
        int cpuid = nm.cpuid;

        // teams can send early messages that'll rise on the queue while they are still processing current request
        if (cpu_track_shared[cpuid] == 1){
#if DEP_VERS > 0
            cout << "Skipping msg from cpu #" << dec << cpuid << " because already processing a message for them " << endl;
#endif
            continue;
        }

        netMsgs.pop();
        int msg_type = nm.msg >> 13;
        struct Player *player;

        if (msg_type == 1) {
            //if (cpu_to_player.find(cpuid) == cpu_to_player.end()){  //not found
            int player_id = nm.msg & 0x1FFF;

            if (player_to_cpu.find(player_id) == player_to_cpu.end()){  //player_id not found, initialize a player
                if (unusedPlayerSlots == 16) {
                    sendMessage(cpuid, ERROR, 1);
                    continue;
                }
                player = initPlayer(unusedPlayerSlots, player_id);
                unusedPlayerSlots++;
                cpu_to_player[cpuid] = player_id;
                player_to_cpu[player_id] = cpuid;
            } else {
                player = getPlayerByPId(player_id); // get player values
                if (player->zombie){
                    player->zombie = false;
                    uint16_t tmp = player->session_id;
                    player->session_id = session_data[cpuid] & 0x1FFF;
#if DEP_VERS > 0
                    cout << "RECONNECTED:: player " << player->id << " on cpu #" << cpuid
                         << " is granted a new session id " << hex << player->session_id
                         << " changed from " << hex << tmp << endl;
#endif
                }
                cpu_to_player[cpuid] = player_id;
                player_to_cpu[player_id] = cpuid;
            }
#if DEP_VERS > 0
            cout << "\t\t\t\t\e[36mDirect (" << getpid() << "): PID=" << dec << player_id << " #" << cpuid
                << " RECVd msg (" << hex <<  msg_type << "-" << msgType(true, msg_type) << ")"
                << " ->> SENDING ACK for Session# " << player->session_id << endl;
#endif
            sendMessage(cpuid, ACK, player->session_id);

            continue;

        } else {
            // ERROR: they sent a 2 or msgtype but haven't sent a 1

            if (cpu_to_player.find(cpuid) == cpu_to_player.end()){
                sendMessage(cpuid, ERROR, 2);
#if DEP_VERS > 0
                cout << "\t\t\t\tCPU " << dec << cpuid << " failed to request a player " << endl;
#endif
                //network_send_to[cpuid] = (7 << 13) | 0x1;
                continue;
            }
            bool player_not_found = true;

            if (msg_type == 4 || msg_type == 5){
                uint16_t session_id = nm.msg & 0x1FFF;
                player = getPlayerByCookie(session_id);
                if (player == NULL){
                    sendMessage(cpuid, ERROR, 64);
                    continue;
                }
            } else {
                int player_id = cpu_to_player[cpuid];
                player = getPlayerByPId(player_id);
            }

            if (msg_type == 6){
                if (zombie_zone[cpuid]== 1){
                    zombie_zone[cpuid] = 0;
                    player->zombie = true;
#if DEP_VERS > 0
                    cout << "CPUID " << dec << cpuid << " is a ZOMBIE!!!" << endl;
#endif
                    continue;
                }
            }
            if (msg_type == 3){
                sendMessage(cpuid, BANK_RESULT, player->bank );
                continue;
            }
            if (msg_type == 2){
                if (player->done_with_hand){
                    netMsgs.push(nm);
                    continue;
                }
                if (player->cards == 0){
                    uint16_t playershand = build_hand();
                    player->cards = playershand;
                }
            } else if (player->done_with_hand){
                cout << "ERROR message #4 Recv'd msg_type > 2 but already started hand. " << dec << cpuid << flush << endl;
                sendMessage(cpuid, ERROR, 4);
                continue;
            }
            if (player->zombie){
                sendMessage(cpuid, ERROR, 32);
            }
            if (msg_type == 4 || msg_type == 5){
                if (!player->started_hand){
                    sendMessage(cpuid, ERROR, 128);
                }
            }
        }

        child_start_time = std::chrono::steady_clock::now();

//        cout << "\t\t\t\tMesg type IN: " << hex << msg_type << " payload: " << (nm.msg & 0x1FFF)
//             << " Player Bank " << dec << player->bank
//             << endl;

        setMemory(nm.msg, gol_netmsg );

        if (player->cards == 0) {
            continue;
        }
        if (msg_type == 4){
          uint16_t cardval = currentcards.front();
          currentcards.pop();
          setMemory(cardval, gol_new_card);
        }
        setMemory(dealer_cards, gol_dlr_cards);
        setMemory(dealer_result, gol_dlr_result);
        setMemory(player->cards, gol_player_cards);
        setMemory(player->result, gol_player_result);
        setMemory(player->bank, gol_player_bank);
        setMemory(player->bet, gol_player_bet);
        setMemory(player->session_id, gol_session_id);
        setMemory(player->id, gol_player_id);

        //setMemory(nm.msg, NETMSG )
        /* set values to process next request */
        child_pid = fork();

        if (child_pid < 0) exit(4);

        if (!child_pid) {  // child_pid == 0, i.e., in child

            /* Child process. Close descriptors and run free. */
            //printf("\t\t\tlaunch cnt = %d Child pid == %d, but current pid = %ld\n", child_pid, (long) getpid());
            fflush(stdout);
            return 1;

        }

        /* Parent. */
        childPidSet.insert(child_pid);
        for (int i =0; i < 50; i++){
            int pidkey = children_shared[i];
            if (pidkey > 0 && childPidSet.find(pidkey) != childPidSet.end()){ // found it
                childPidSet.erase(pidkey);
                children_shared[i] = 0;
            }
        }

        // cout << "EST " << childPidSet.size() << " children running ";
        if (childPidSet.size() >= 16){
            cout << "\t\t\t\tExceeded limit, waiting for any child  to finish...";
            wait(NULL);
            cout << "Resuming execution!" << endl;
        }

    }

   return 0;

}
int doTheShuffle(){
    int status = 0;
    while (true){
        shuffle_start_time = std::chrono::steady_clock::now();

        // if dealer has too many future cards, it will not pick up more cards, the shuffler wait until it does.
        if (card_data[25] > 0 ) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10*1000));
            if (card_data[25] > 0 ) {
                //cout << "Too many cards generated, waiting until added by dealer " << endl;
            }
            continue;
        }

        int child_pid = fork();

        if (child_pid < 0) exit(4);

        if (!child_pid) {  // child_pid == 0, i.e., in child

            /* Child process. Close descriptors and run free. */
            //printf("\t\t\tlaunch cnt = %d Child pid == %d, but current pid = %ld\n", child_pid, (long) getpid());
            int xpos = 15;
            int bitval =7;
            uint16_t tmp;

            if (session_data[16] == 0){
                char buf[3];
                size_t randomResult = getrandom(buf, 2, 0);

                if (randomResult == 2){
                    tmp = (buf[0]<<8) | buf[1];
                } else {
                    cout << "ERROR getting initial seed for random values." << endl;
                    time_t now = time(NULL);
                    tmp = (uint16_t) (now & 0xFFFF);
                }

                cout << "\t\t\tGenerated SEED is 0x" << hex << tmp << " 0b" << bitset<16>(tmp).to_string() << endl;

            } else {
                tmp = session_data[16];
            }

            string bitstr = bitset<16>(tmp).to_string();

            for(char& c : bitstr) {

                if (c == '0'){
                    bitval = 6;
                } else {
                    bitval = 7;
                }
                imp->setcell(4058+xpos*11, 596, bitval);
                imp->setcell(4058+xpos*11+1, 596, bitval);
                xpos--;
            }

            fflush(stdout);

            return 1;

        }

        /* Parent. */

        /* Get and relay exit status to parent. */
        int waitedpid = waitpid(child_pid, &status, 0);
        if (waitedpid < 0) {
            printf("\t\tExiting Parent %d with 6\n", child_pid);
            exit(6);
        }


    }

    return 0;

}

void mutateRandomToCardVals(){

    uint16_t cardBuckets[16] = {0,8,8,8,  8,8,8,8, 8,8,8,8, 8,8,0,0};
    int counter = 1;
    for (int j=0; j < 26;j++){
        uint16_t tmpCards = getValue(18 + j);
        uint16_t outCards = 0;
        for (int k=0; k < 16; k+=4){
            uint16_t cardVal = (tmpCards >> k) & 0xF;
            uint16_t origCardVal = cardVal;
            while (cardBuckets[cardVal] == 0){
                cardVal = origCardVal ^ counter;
                cardVal = cardVal & 0xF;
                counter++;
            }
            cardBuckets[cardVal]--;
            cardVal = cardVal << k;
            outCards = outCards | cardVal;
        }
        card_data[j] = outCards;
    }
}

int main(int argc, char *argv[]) {

   signal(SIGINT, signal_handler);
   if (getenv("ROUND_ID")){
       round_id = getenv("ROUND_ID");
   } else {
       round_id = "9999";
   }


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
   int fc = 0 ;

   /**
   * Start of main event loop
   */

   bool shuffling = shuffler > 0;
   bool resetShared = resetshared > 0;
   if (!init_game_data(shuffling, resetShared)){
       return 99;
   }
   initPcapFile();

   if (shuffling){
       //cout << "Starting up Shuffler @ " << dec << getpid() << "";
       int inchild = doTheShuffle();
       if (inchild == 1){
           //cout << "Doing the shuffle, the time warp shuffle." << endl;
           //shuffle_y_adj = 33;
       } else {
           cout << "exit parent shuffler." << endl;
           return 0;
       }

   } else {
       cout << "\t\t\t\tStarting up Dealer @ " << dec << getpid() << " for Round #" << round_id << endl;
       if (wait_for_msg() == 0){
           cout << "Parent DONE!!" << endl;
           return 0;
       } else {
           //printf("\t\t\t\tWorker child starting\n");
       }

   }


   for (;;) {

      if (benchmark)
         cout << timestamp() << " " ;
      else
         timestamp() ;
      if (!quiet && imp->getGeneration() > 0) {
          if (shuffling){
              cout << "\t\t\t\tShuffle (" << dec<< getpid() << "):" << imp->getGeneration().tostring() << endl ;
          } else {
              //cout << "Worker action ==  " << dec << getValue(gol_action) << endl;
              cout << "\t\t\t\tWorker (" << dec<< getpid() << "):" << imp->getGeneration().tostring() << endl ;
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

      if (shuffling){
          uint16_t actionVal = getValue(1);
#if DEP_VERS > 0
          if (!quiet) {
              cout << "\t\t\tShuffle: action val/fndcnt = " << dec << actionVal;
              cout << " Sessions [ ";
              for (int j=2; j < 18;j++){
                  uint16_t sval = getValue(j);
                  cout << hex << sval << " ";
              }
              cout << " ] " << endl;
              cout << "\t\t\t\t Card Rands [ ";
              for (int j=18; j < 31;j++){
                  uint16_t cards = getValue(j);
                  cout << hex << cards << " ";
              }
              cout << " ]" << endl;

          }
#endif
          if (actionVal == 13){
              break;
          }

      } else {
          uint16_t actionVal = getValue(gol_action);
          //cout << "action val = " << actionVal << endl;
          if (actionVal >= 5){
              break;
          }
      }
   } // main for loop

   // on the way out
   if (shuffling){
       string bitstr;
       mutateRandomToCardVals();

          // check that no card values are 0, 14, 15 and put into buckets for rank frequency check
          uint16_t card_buckets[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

          for (int j=0; j < 26;j++){
              uint16_t cards = card_data[j];
              for (int scnt=0; scnt < 16 ; scnt+=4){
                  uint16_t card = (cards >> scnt) & 0xF;
                  card_buckets[card]++;
                  if (card == 0 || card == 14 || card == 15){

                      cout << "\e[31mERROR: the group of cards " << hex << cards
                           << " generated by " << hex << session_data[16]
                           << " has an improper value" << endl;
                      for (int x=0; x < 13;x++){
                          cout << "0x"<< hex << card_data[x] << " ";
                      }
                      cout << endl;

                      for (int xpos=15; xpos >= 0; xpos--){

                            if (imp->getcell(4058+xpos*11, 596) == 7){
                                bitstr += '1';
                            } else {
                                bitstr += '0';
                            }
                      }

                      unsigned long n = std::bitset<16>(bitstr).to_ulong();
                      cout << "\t\t\t\tNext SEED  " <<  bitstr << " " << n << endl;
                      session_data[16] = (uint16_t) n;
                      return 39;
                  }
              }
              //cout << "\tcard_data[" << dec << j << "] = 0x" << hex << getValue(26 + j) << ";" << endl;
          }
          // check for correct number of each rank of cards.
          for(int j=1; j < 14; j++){
              if (card_buckets[j] != 8){
                  cout << "\e[31mERROR: improper number of cards, found " << card_buckets[j] << " of rank " << dec << j   << "\e[0m" << endl;
                  return 49;
              }
          }

//          for (int j=0; j < 16;j++){
//            cout << "\tsession_data[" << dec << j << "] = 0x" << hex << (getValue(40+j) & 0x1FFF) << ";" << endl;
//          }
          // copy sessions to shared area
          for (int j=0; j < 16;j++){
              session_data[j] = getValue(2+j) & 0x1FFF;
          }

        // save current seed in session_data[16]
        for (int xpos=15; xpos >= 0; xpos--){
            if (imp->getcell(4058+xpos*11, 596) == 7){
                bitstr += '1';
            } else {
                bitstr += '0';
            }
        }

        unsigned long n = std::bitset<16>(bitstr).to_ulong();
        //cout << "\t\t\t\tNext SEED  " <<  bitstr << " " << n << endl;
        session_data[16] = (uint16_t) n;
        writepat(-1) ;
#if DEP_VERS > 0
        if(!quiet){
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            long duration = chrono::duration_cast<chrono::milliseconds>(end - shuffle_start_time).count();
            cout << "\t\t\t\t\e[38;5;10mShuffle Completed:" << imp->getGeneration().tostring() << " in " << dec << duration << "s \e[0m" << endl ;
        }
#endif

   } else {

       int player_id = getValue(gol_player_id);
       int cpuid = player_to_cpu[player_id];
       uint16_t outmsg = getValue(gol_outmsg);
       //cout << "\n\t\t\t\tCPUID=" << cpuid << "  outmsg=" << outmsg << endl;

       for (int i =0; i < 16;i++){
           Player p = player_data[i];
           if (p.id == player_id && !p.done_with_hand){
               //network_send_to[cpuid] = outmsg;
               //cout << "\n\t\t\t\tCPUID=" << cpuid << "  outmsg=" << outmsg << endl;
               int msg_type = outmsg >> 13;
               if (msg_type == 1){
                   player_data[i].session_id = (outmsg & 0x1FFF);
               }
               string wl = "";

               uint16_t incMsgType = getValue(gol_incoming_type);
               player_data[i].cards = getValue(gol_player_cards);
               player_data[i].result = getValue(gol_player_result);

//               cout << "\t\t\tBANKS tmp=" << tmpbank << " new " << player_data[i].bank
//                    << " tmpcheck=" << dec << getValue(gol_action+2)
//                    << endl;

               if (msg_type == 6){
                   uint16_t tmpbank = player_data[i].bank;

                   player_data[i].bank = getValue(gol_player_bank);
                   string bitstr = bitset<16>(getValue(gol_action+2)).to_string();
//                   cout << " " << dec << getValue(gol_player_result)
//                        << " " << (getValue(gol_player_cards) & 0x1F)
//                        << " DEAL " << getValue(gol_dlr_result) << " " << dealer_result << " "
//                        << "bet=" << getValue(gol_player_bet) << " tmpc=" << dec << getValue(gol_action+2)
//                        <<  " 0b" << bitstr
//                        << endl;
                   player_data[i].done_with_hand = true;
//                   if (player_result <= 21)
//                        if (dlr_result > 21)
//                            player_bank = player_bank + player_bet
//                            tmp_card = 111
//                        if (dlr_result < player_result)
//                            player_bank = player_bank + player_bet
//                            tmp_card = 222
//                    # && is not working correctly
//                    if (dlr_result <= 21)
//                        if (player_result < dlr_result)
//                            player_bank = player_bank - player_bet
//                            tmp_card = 999
#if DEP_VERS > 0
                   uint16_t player_result = getValue(gol_player_result);

                   bool win = false;
                   if (dealer_result == 255){
                       if (player_result < 21){
                           win=false;
                       }
                   } else if (player_data[i].result <= 21){
                       if (dealer_result > 21 || dealer_result < player_result){
                           win = true;
                       }
                   }

                   if (tmpbank < player_data[i].bank){
                       player_data[i].lastHandResult = 4;
                       wl = " \e[32mwon\e[36m";
                       if (!win){
                           cout << "\e[31mERROR: won money but did not calculate a win. "
                                << dec << getValue(gol_player_result)
                                << " " << (getValue(gol_player_cards) & 0x1F) << " DEAL "
                                << getValue(gol_dlr_result)
                                << "tmpcheck=" << dec << getValue(gol_action+2) << endl;
                       }
                   } else if (tmpbank > player_data[i].bank){
                       player_data[i].lastHandResult = 1; // lost
                       if (player_data[i].result > 21){
                           wl = " \e[31mBUSTo\e[36m";
                       } else {
                           wl = " \e[31mlost\e[36m";
                       }

                       if (win){
                           cout << "\e[31mERROR: lost money but did not calcualte a loss. "
                                << dec << getValue(gol_player_result)
                                << " " << (getValue(gol_player_cards) & 0x1F) << " DEAL "
                                << getValue(gol_dlr_result)
                                << "tmpcheck=" << dec << getValue(gol_action+2) << endl;
                       }
                   } else {
                       player_data[i].lastHandResult = 2; // push
                       wl = " \e[33mpush\e[36m";
                       if ((dealer_result != player_data[i].result) && (dealer_result != 255 && player_data[i].result != 21)){
                           cout << "\e[31mERROR: no change in bank but did not calcualte a push."
                                << dec << getValue(gol_player_result)
                                << " " << (getValue(gol_player_cards) & 0x1F) << " DEAL "
                                << getValue(gol_dlr_result)
                                << "tmpcheck=" << dec << getValue(gol_action+2) << endl;
                       }
                   }
#endif
               }

               // if incoming == BET and outgoing == DEAL, then set started_hand
               if (incMsgType == 2 && msg_type == 3 ){
                   player_data[i].started_hand = true;
               }
               if (incMsgType == 2 && msg_type == 7){
                   player_data[i].bet = 0;
               } else {
                   player_data[i].bet = getValue(gol_player_bet);
               }

               string tmpstr = string(player_data[i].cardTrack);
               if (tmpstr.empty() ){
                   tmpstr += convertCard(player_data[i].cards >> 8 & 0xF);
                   tmpstr += convertCard(player_data[i].cards >> 12 & 0xF);
                   strcpy(player_data[i].cardTrack,tmpstr.c_str());
               } else if (incMsgType == 4) {
                   tmpstr += convertCard(getValue(gol_new_card) & 0xF);
                   strcpy(player_data[i].cardTrack,tmpstr.c_str());
               }

#if DEP_VERS > 0
               std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
               long duration = chrono::duration_cast<chrono::milliseconds>(end - child_start_time).count();

               int initCards = (player_data[i].cards & 0xFF00) >> 8;
               int hitCard = 0;
               if (incMsgType==4){
                   hitCard = getValue(gol_new_card) & 0xF;
               }

               string dealer_result_out = dealer_result == 255 ? "BLACKJACK!!!!" : to_string(dealer_result);
               //printf("\t\t\t\t\e[31mDEALER CHILD: outmsg = 0x%x, outmsg type = 0x%x  pay_load=0x%x  session_pos=%d BET=%d \e[0m\n", outmsg, msg_type, (outmsg & 0x1FFF), gol_session_id, player_data[i].bet );
               cout << "\t\t\t\t\e[36mWORKER (" << getpid() << "): ID=" << dec << player_id << "  #" << dec <<cpuid
                    << " in " << dec << duration << "s "
                    << " RECVd msg (" << hex <<  incMsgType << "-" << msgType(true, incMsgType) << ")"
                    << " " << hex << getValue(gol_netmsg)
                    << " DLR [ Shows=" << hex << dealer_cards << "\e[36m Has=" << dealerTrackCards << "\e[36m"
                    << " Tot= " << dec << dealer_result_out << " ]"
                    << " \n\t\t\t\t  ->> SENDING \e[33m(" << msg_type << "-" << msgType(false, msg_type) << ") \e[36m"
                    << " to #" << dec <<cpuid << " " << hex << outmsg
                    << wl << "\e[36m"
                    << " PLAYR [ Cards=" << hex << (initCards & 0xFF) << " + " << hex << hitCard
                    << " Has=\e[33m" << string(player_data[i].cardTrack)
                    << " \e[36mTot=" << dec << (player_data[i].cards & 0x1F)
                    << " bet=" << dec << player_data[i].bet
                    << " bank=" << dec << player_data[i].bank
                    << " sess=" << hex << getValue(gol_session_id) <<  "] \e[0m"
                    << flush << endl;
#endif
               recordNetworkComm(cpuid, SERVER_ID, outmsg);
               network_send_to[cpuid] = outmsg;
           }
       }

       for(int i=0; i < 50;i++){
           if (children_shared[i] == 0){
               children_shared[i] = getpid();
               break;
           }
       }
       cpu_track_shared[cpuid] = 0;

   } // end else shuffler


   if (maxgen >= 0 && outfilename != 0)
      writepat(-1) ;
   exit(0) ;
}

