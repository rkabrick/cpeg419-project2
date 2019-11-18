# cpeg419-project2
This is a class project for CPEG 419: Computer Networks

The purpose of this project is to send a file over a network with loss, but no errors, using UDP
It is an implementation of the stop-and-wait protocol to recover from packet/ACK loss
The loss is statistically simulated, in order to be able to stress the implementation.

This project is all raw C, it should compile and run on any linux system with make