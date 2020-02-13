# Ultimatum Game Server
A server enabling multiple clients to play the [ultimatum game](https://www.cs.mcgill.ca/~rwest/wikispeedia/wpcd/wp/u/Ultimatum_game.htm), a well-known experimental economics game. It records the result of each game in a csv file.

I wrote this program as a learning project. It gave me the chance to:

* Build on the basic C I learned in [CS50](https://www.edx.org/course/cs50s-introduction-to-computer-science)
* Practice socket programming using system calls
* Learn about basic POSIX threading which is used to handle multiple clients

## Screenshots
Server view showing number of connections and games completed. Results are recorded to csv file. It is possible to use a previous csv file to resume data collection.

![server](https://github.com/galenlim/ultimatum-game/blob/master/docs/screenshots/server.jpg)

___

Game play when offer is accepted

![client pair accept](https://github.com/galenlim/ultimatum-game/blob/master/docs/screenshots/clientaccept.jpg)

___

Game play when offer is rejected

![client pair reject](https://github.com/galenlim/ultimatum-game/blob/master/docs/screenshots/clientreject.jpg "Offer rejected")

___

Helpful references:
* Basic server-client model - https://www.cs.rpi.edu/~moorthy/Courses/os98/Pgms/socket.html	
* Threading tutorial - http://www.mario-konrad.ch/blog/programming/multithread/tutorial-01.html
* Comprehensive socket programming book - https://beej.us/guide/bgnet/html//index.html 
* Buffering of C streams - https://www.ibm.com/support/knowledgecenter/SSLTBW_2.4.0/com.ibm.zos.v2r4.cbcpx01/buff.htm	
