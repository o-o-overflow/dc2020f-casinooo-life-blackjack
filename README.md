# DC 2020 Finals
## Casinooo Life - Blackjack  
### SPOILERS - This readme contains spoilers if you want to play it with limited spoilers check it out at archive.ooo
Have you always wanted to be a professional gambler?  Are you ready to enjoy the easy casinooo life? Come and play our hot new blackjack game. We are the only casino around that will let you 'play it YOUR way' with your own blackjack bot. You have chance to beat the house and WIN IT ALL. For a safer experience, play at home with your own dealer (be sure you still tip them). Please see your interface for details. Some limitations apply. All dealer decisions final. If you have a gambling problem, please get help.

###Overview
This service is comprised of 3 main components. A controller, a shuffler, and a dealer. Each of the components are Game of Life (GoL) components and are nitted together, in a somewhat Frankenstein fashion, using a heavily modified version of bgolly. 
The components communicate using multiple regions of shared memory. 
![alt text](https://github.com/o-/[reponame]/blob/[branch]/image.jpg?raw=true)
 
###The Controller
As a professional BJ player controller, you must implement a Game of Life (GoL) pattern that will receive and respond to OOO's blackjack dealer.   
Each controller is stored in */tmp/inputs*. At the end of each round, the script will update the controller from /tmp/inputs and copy the file to /gol/inputs for use by the bgolly-player.

A controller can be complete
The initial controller uses the send/read opcodes to communicate with the dealer. The information is written to a storage area near the opcode area, which is read by bgolly-player and sent to the bgolly-dealer.

###Communication Protocol
The controller and the bot use a simple 16-bit messages to communicate with one another. The first 3 bits of the message is the message type, the last 13 bits are used in various ways. 
```
Msg# To Dealer           From Dealer
 1   INIT|PlayerId       ACK|SessionCookie
 2   BET|Amount          BANK RESULT
 3   BANK REQUEST        DEALT|Dealer showing card|Player Card 1|Player Card 2
 4   HIT|SessionCookie            
 5   STAY|SessionCookie
 6                      RESULT| Card or New Bank
 7                      ERROR|Error Code
```
Each message sent from a controller to the dealer, is tagged with a "hardware" CPU id that correlates to the team that created the controller. 

**HINT**: Although the reference implementation uses a playerId that's provided by the CPU it's not mandatory the player Id can be any 13-bit value. Without changing it, it is possible for another team to send an init that gives them the session cookie for a different player.


 
  




  

