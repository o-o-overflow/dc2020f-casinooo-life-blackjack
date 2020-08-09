# DC 2020 Finals
## Casinooo Life - Blackjack  
### SPOILERS - This readme contains spoilers if you want to play it with limited spoilers check it out at archive.ooo
Have you always wanted to be a professional gambler?  Are you ready to enjoy the easy casinooo life? Come and play our hot new blackjack game. We are the only casino around that will let you 'play it YOUR way' with your own blackjack bot. You have chance to beat the house and WIN IT ALL. For a safer experience, play at home with your own dealer (be sure you still tip them). Please see your interface for details. Some limitations apply. All dealer decisions final. If you have a gambling problem, please get help.

###Overview
This service is comprised of 3 main components. A controller, a shuffler, and a dealer. Each of the components are Game of Life (GoL) components and are nitted together, in a somewhat Frankenstein fashion, using a heavily modified version of bgolly. 
The components communicate using multiple regions of shared memory. 

![alt text](https://github.com/o-o-overflow/dc2020f-casinooo-life-blackjack/blob/master/overview-diagram.png?raw=true)
 
###The Controller
As a professional BJ player controller, you must implement a Game of Life (GoL) pattern that will receive and respond to OOO's blackjack dealer.   
Each controller is stored in */tmp/inputs*. At the end of each round, the script will update the controller from /tmp/inputs and copy the file to /gol/inputs for use by the bgolly-player.

![alt text](https://github.com/o-o-overflow/dc2020f-casinooo-life-blackjack/blob/master/controller.png?raw=true)

A controller can be complete
The initial controller uses the send/read opcodes to communicate with the dealer. The information is written to a storage area near the opcode area, which is read by bgolly-player and sent to the bgolly-dealer.

![alt text](https://github.com/o-o-overflow/dc2020f-casinooo-life-blackjack/blob/master/network-input.png?raw=true)
  

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
Each message sent from a controller to the dealer, is tagged with a "hardware" CPU id that correlates to the team that created the controller. 

A maximum of 16 player spots, which means once 16 unique player tokens are supplied via INIT. Once all 16 are initialized. 

Although the reference implementation uses a specific player token that's provided by the CPU, the player token is defined by the 13-bit value sent in the INIT message. Without changing it, it is possible for another controller to spoof messages and cause a player to send messages. 

### Potential exploits/strategy

1. A controller should change the behavior of the reference implementation to prevent it from setting max bet and hitting until it busts.

2. A controller can use multiple players in a single hand.

3. A controller can spoof a player token and send BET, HIT, STAY messages as the victim player

4. Move "secret" information into the LifeLock area that will not be included in the REDACTED version of the patterns, which is what is sent to the other teams.

5. A controller can set a player token when sending the first init message for a player and should create a random value

6. By sending a bet size that's too large and causing a betting error, the return value includes information that indicates the card value the player will receive  

7. A controller can set cells that allow for a hot-restart of the controller

8. Cookie spoofing is possible 

9. By using the CRC controller restart, a player is set to zombie mode and on reacquisition of the player it is given a new session cookie.

    




 
  




  

