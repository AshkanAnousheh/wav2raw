## Requirements
* pthread
* cmake
* git
* gcc

## How to use it?
Just use these commands in a row

```bash
# configure project
cmake -B build -S .

# build it
cmake --build build

# run it
./build/wav2raw <file_path> <IP> <port> <delay between chunks in microsecond>

# for example
./build/wav2raw ./440hz_sine_16bit_48K.wav 127.0.0.1 12345 1000

# to schedule it in real-time mode inside linux this
sudo chrt -f 99 ./build/wav2raw ./my.wav 192.168.8.40 510 2500

```