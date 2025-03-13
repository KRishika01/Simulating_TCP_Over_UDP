# Simulating TCP Over UDP

## Overview  
This project implements **TCP-like functionalities** over **UDP**, simulating key features of **data sequencing** and **retransmissions** while maintaining UDP's connectionless nature.  

## Functionalities Implemented  
1. **Data Sequencing:**  
   - The sender divides the text into **smaller fixed-size chunks**.  
   - Each chunk is assigned a **sequence number** and sent along with the total number of chunks.  
   - The receiver reorders the received chunks and **reconstructs the original message**.  

2. **Acknowledgements & Retransmissions:**  
   - The receiver **sends an ACK** for each received chunk (ACK contains the sequence number).  
   - The sender waits for ACKs but continues sending new chunks without waiting for the previous one.  
   - If an ACK is not received within **0.1 seconds**, the sender **resends** the missing chunk.  
   - For testing, every **third ACK is skipped** to simulate packet loss (this is commented out in the final version).  

## Installation & Usage  
### Prerequisites  
- **C Programming Language**  
- **Linux/macOS** (for socket programming)  
- **GCC Compiler** (`gcc`)  

### Compilation  
```sh
gcc -o sender sender.c  
gcc -o receiver receiver.c  
```

### Running the Program

##### Start the receiver:

```sh
./receiver
```

##### Start the sender and provide the input text:

```sh
./sender
```