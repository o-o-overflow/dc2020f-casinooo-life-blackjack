# DC 2020 Freinals
## Casinooo Life - Blackjack
### SPOILERS - This readme contains spoilers if you want to play it with limited spoilers check it out at archive.ooo
Challenge Author: TrickE with some help from Zardus

Have you always wanted to be a professional gambler?  Are you ready to enjoy the easy casinooo life? Come and play our hot new blackjack game. We are the only casino around that will let you 'play it YOUR way' with your own blackjack bot. You have chance to beat the house and WIN IT ALL. For a safer experience, play at home with your own dealer (be sure you still tip them). Please see your interface for details. Some limitations apply. All dealer decisions final. If you have a gambling problem, please get help.

###Overview

This service is comprised of 4 main components. A controller, a shuffler, player seats, and a dealer. The controller, shuffler, and dealer are Game of Life (GoL) components and are nitted together, in a somewhat Frankenstein fashion, using a heavily modified version of bgolly. The player seats are 16 data objects stored in memory inside the bgolly-dealer used the track the hand-to-hand play information (e.g., money in bank, wager size, current cards, etc.).   
The components controller and dealer communicate run inside different processes that communicate using multiple regions of shared memory. 

![alt text](https://github.com/o-o-overflow/dc2020f-casinooo-life-blackjack/blob/master/overview-diagram.png?raw=true)


###The Controller

As a professional BJ player controller, you must implement a Game of Life (GoL) pattern that will receive and respond to OOO's blackjack dealer. The controller controls a player's seat by sending an initialization message to the dealer. The controller creates a unique identifier in its initialization message that will identify a player until the end of a round.  

Each controller is stored in */tmp/inputs*. At the end of each round, the script will update the controller from /tmp/inputs and copy the file to /gol/inputs for use by the bgolly-controller.

![alt text](https://github.com/o-o-overflow/dc2020f-casinooo-life-blackjack/blob/master/controller.png?raw=true)

The initial controller uses the send/read opcodes to communicate with the dealer. Inside the GoL pattern, the message to be sent is written to a storage area near the opcode area, which is read by bgolly-controller and sent to the bgolly-dealer.

![alt text](https://github.com/o-o-overflow/dc2020f-casinooo-life-blackjack/blob/master/network-input.png?raw=true)
  
The bgolly-controller is responsible for writing out a READACTED version of the pattern, which is shared with the other teams. This READACTED version retains everything but the portion of the pattern denoted by the  LifeLock area.

###Communication Protocol

The controller and the bot use a simple 16-bit messages to communicate with one another. The first 3 bits of the message is the message type, the last 13 bits are used in various ways. 
```
Msg# To Dealer           From Dealer
 1   INIT|PlayerToken       ACK|SessionCookie
 2   BET|Amount          BANK RESULT
 3   BANK REQUEST        DEALT|Dealer showing card|Player Card 1|Player Card 2
 4   HIT|SessionCookie            
 5   STAY|SessionCookie
 6                       RESULT| Card or New Bank
 7                       ERROR|Error Code
```
Each message sent from a controller to the dealer, is tagged with a "hardware" CPU ID, which is the same as a team's ID. Thus, the teams' don't controller a particular player, but instead operate a controller that is capable of controller 1 or more player seats. 

A maximum of 16 player seats exists in the dealer, which means once 16 unique player tokens are supplied via INIT no more players are available. For example, if a controller were to send 16 INIT messages with unique player tokens before any other controller, then none of the other controllers could play that round without guessing one of the identifiers.

Although the reference implementation uses a single player token that's provided by the bgolly-controller and is the same as their CPU ID, the player token is defined by the 13-bit value sent in the INIT message. Without changing it, it is possible for another controller to spoof messages and cause a player to send messages. 

### Potential exploits/strategy

1. A controller should change the behavior of the reference implementation to prevent it from setting max bet and hitting until it busts.

2. A controller can use multiple players in a single hand. By controlling 1 additional player, the controller can send a BET message and use the "Dealer showing card" using one player and make a bet adjustment on the second player to maximize their odds of winning (i.e, bet small when a T,J,Q,K is showing). Obtaining multiple players can be sped up by implementing custom hardware that sends the value to the network out location faster than the controller's GoL CPU can. 

3. A controller can spoof an already initialized seat by providing the same player token used in the initialization and send a BET as the victim player. In addition, the attacker can send an INIT message with the already established player token to receive the CPU's generated session cookie. Using the session cookie, the controller can then send HIT and STAY messages as the victim player.

4. To prevent other teams from understanding your strategy, sensitive information should be moved into the LifeLock area, which is not be included in the REDACTED version of the patterns, which is what is sent to the other teams.

5. A controller can set a player token when sending the INIT message for a player, which should be sufficiently unique that it cannot be easily guessed by the other controllers, which will prevent them from spoofing messages that impacts their player.

6. A controller can send an intentionally erronous BET message (i.e., the bet size exceeds their bank). In the error message information about the player's cards, which are set at INIT, are leaked. Although it is not the raw value of the cards, it is possible to determine how many cards 10 or greater will be in the player's starting hand.    

7. A controller can set cells that allow for a hot-restart of the controller. If a simple CRC check of the cells in the zone (0,0 to 600,600) equals 0 in a controller that's loaded at the start of a round, the controller will reboot and load a newly updated controller in the middle of a round. 

8. The dealer uses a 16-bit xor shift for generating its random values from a seed. 
```
xs ^= xs << 7;
xs ^= xs >> 9;
xs ^= xs << 8;
```
Thus, with just the session cookie provided to a player its possible to brute-force the sequence, outside the GoL, and load a new controller that can spoof the session cookies set by the CPU and send HIT or STAY messages as a spoofed player.

9. To combat the atack in item 8, a player can reset their own GoL controller using the CRC check method. This causes the player they are controlling to be placed into a zombie mode. On re-initialization after the reloading, the zombie player is given a new session cookie, which is significantly harder to discover. 

    




 
  




  

