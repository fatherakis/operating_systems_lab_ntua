## Riddle

A fun binary program with progressively more difficult puzzles. You can only run it on a Linux system. There is a savefile once you run it so you can keep your progress! Its whole scope is to learn some basic reverse-engineering on how can a binary file behave and how to manipulate it accordingly.  As part of the course, the first 15 Challenges are mandatory.

* [Challenge 0](#challenge-0)  
* [Challenge 1](#challenge-1)
* [Challenge 2](#challenge-2)
* [Challenge 3](#challenge-3)
* [Challenge 4](#challenge-4)  
* [Challenge 5](#challenge-5)
* [Challenge 6](#challenge-6)
* [Challenge 7](#challenge-7)
* [Challenge 8](#challenge-8)  
* [Challenge 9](#challenge-9)
* [Challenge 10](#challenge-10)
* [Challenge 11](#challenge-11)
* [Challenge 12](#challenge-12)  
* [Challenge 13](#challenge-13)
* [Challenge 14](#challenge-14)


## <div align="center"> Solutions </div>

### Challenge 0: 

Name: "Hello there"\
Hint: "Process run system calls"

As the hint suggests, probably check the system calls. In linux you can check any system call a command/program executes with "strace" command.

for example: `` strace ./riddle``.

From there, examining the results, right before writing "FAIL", we see an attempt to open a ".hello_there" file which doesn't exist and returns -1 ENOENT.

Creating that file with ``touch .hello_there`` and running the program once again, we have completed the first challenge.

### Challenge 1: 

Name: "Gatekeeper"\
Hint: "Stand guard, let noone pass"\
"... I found the doors unlocked. FAIL"

The hint here doesn't really indicate anything for now so we can try strace again.

Again right before the "FAIL" we see a system call:\
``openat(AT_FDCWD, ".hello_there", O_WRONLY) = 4``

Here the flag at openat is write-only which adds up with the hint and "I found the doors unlocked" indicating the file needs to be write only.

If we try and set out created file .hello_there as write only with:\
``chmod 444 .hello_there``\
and run the program again we see we have completed this challenge.


### Challenge 2: 

Name: "A time to kill"
Hint: "Stuck in the dark, help me move on"
\<delay> You were eaten by a grue. FAIL

Just like before, running strace on the program, there is an alarm and a pause call.

Since the program has a SIGALRM our only route is to send ("help it move on") a SIGCONT (for the program to continue) signal through the kill command. For that to happen we also need the pid of the program.

So run the program and inn a new window, run ``ps aux | grep riddle`` to get the pid
and right after ``kill -SIGCONT <pid_from_above>``.

Be sure to do it fast before time runs out.

Just like that the challenge is completed.


### Challenge 3: 

Name: "what is the answer to life the universe and everything?"
Hint: "ltrace"

Here the hint directs to the ltrace command which shows dynamic library calls. This one is fairly simple, the program tries to access an environment variable with:\
```getenv("ANSWER")   = nil```

We need to set the variable through export:\
```export ANSWER = <Google it pls>```

<details>
<summary> Spoiler if you can't use google or prefered search engine. Even ChatGPT.</summary>
    42. The answer is 42.
</details>

Runnig the program again, challenge 3 is completed.

### Challenge 4: 

Name: "First-in, First-out"\
Hint: "Mirror, mirror on the wall, who in this land is the fairest of all?"\
I cannot see my reflection. FAIL

Since there isn't much to go off of, running strace shows another attempt to open a file "magic_mirror". ``openat(AT_FDCWD, "magic_mirror", O_RDWR) = -1 ENOENT (No such file or directory)``

Our first guess here would be to create the file like challenge 0. Doing so, the challenge still fails but the strace shows it continued. After opening the file, it attempts to write 1 byte and read it right after on which step fails.

What probably happens here is, after writing the singular byte the file descriptor moves by 1 byte and its at the EOF. Reading at that point will not return that said byte. So the program probably wants to write a byte into a file and read that said byte exactly after. The issue here is that reading right after writing in a typical file, returns the next byte and not the written one. This applies to all files except FIFO pipes (where the name of the challenge is from).

In FIFO pipes, executing a read right after a write will read from the last just written byte which is exactly what we want. We can create the FIFO file with:\
``mkfifo magic_mirror``\
after which by running the program we have completed the challenge.

### Challenge 5: 

Name: "my favourite fd is 99"\
Hint: "when i bang my head agains the wall it goes: dup! dup! dup!"

As the name suggests we will have to do something with file descriptor 99.

Running strace again we see a new system call:\
``fcntl(99, F_GETFD)                      = -1 EBADF (Bad file descriptor)``

fcntl with the defined argument returns the file descriptor flags in this case of the 99 file descriptor which doesn't exist.

We can manipulate this however with the dup2() command. (Seek man page for command)

riddle-5.c has a short c script to forward fd 1 (stdout) to 99 and run riddle.

Another way through the terminal would be:\
``exec 99>&1; ./riddle``
or even: ``./riddle 99>&1``

where fd>&fd' redirects file descritor fd to fd' 


### Challenge 6: 

Name: "ping pong"\
Hint: "help us play!"\
[2401] PING!\
FAIL

Just like any other riddle we can start with strace and ltrace. ltrace here is helpful. We see 2 created subprocesses and the main program waiting for them to exit with wait().
Since we know that, we can run strace with -f to show child processes too.

From here we can only identify one process reading a 33 file descriptor and another writing into 34. We can't really manipulate the child processes however we can use the fact that they inherit everyting from the parent. Then lets try to redirect the file descriptors like the previous challenge with:\
``strace -f ./riddle 33>&1 34<&1``

Now we can see some new problems. Now they require file descriptors 53 and 54 and have a weird interaction with 33 and 34 descriptors. At this point we understand we need to use pipes. Two in particular with the first one connecting 33 for reading and 34 for writing and the second one 53 reading and 54 for writing.

we achieve this with script [riddle-6.c](riddle-6.c) which we compile and run.

Essentialy, we create 2 pipes and link their ends with dup2. After that, running the program succesfully completes challenge 6.


### Challenge 7: 

Name: "What's in a name?"\
Hint: "A rose, by any other name..."

Here again through strace we see a new lstat function accessing our old file .hello_there and a non-existent one ".hey_there". Creating the new file with ``touch .hey_there`` and running everything again we see a comparison between 2 numbers. Since lstat returns information about a file, thats where the number probably comes from. We can check this using ``stat <file_name>`` command which returns all file system status. With our results, the numbers compared by riddle are the Inode numbers of the files. In order to make them the same we need to link the 2 files together either through softlink or hardlink.

The easiest and fastest way is with:
``ln .hello_there .hey_there``

after which the challenge is completed.

### Challenge 8: 

Name: "Big Data"\
Hint: "Checking footers"\
Data files must be present and whole. FAIL

Again through strace, the program attempts to access a non-existent "bf00" file multiple times. Creating this file and running strace, the program now uses lseek for the file. lseek repositions the file offset of the file descriptor to a specific value (flag is SEEK_SET). In riddle, lseek targets specific values and reads 16 bytes which are obviously non-existent since the file is empty. However, if these values (the fd offset) surpass the EOF, the file's size is extended and for this exact reason the program writes one single x in stdout after each attempt.

This mostly means that extending the file to the last accessed offset should be good enough. The last offset is 1073741824 so a simple truncate with an offset should be ok?

Running riddle, the challenged isn't finished yet. Instead, this time it requires the same for bf01 ... and then for 02,03,... 

Thus make a script [riddle-8.c](riddle-8.c) to repeat this process 9 times for all files bf00 - bf09. And just like that the challenge is done. 

> [!Note]
> I didn't think of this: You could simply hardlink all files bf01 - bf09 to bf00 and be done without scripts.


### Challenge 9: 

Name: "Connect"\
Hint: "Let me whisper in your ear"\
I am trying to contact you... 


strace again shows an open socket with fd 4 and an attempt to connect to port 49842.

So a reasonable solution would be to listen on that port with netcat (command is ``nc``).
``nc -l 49842``

Answer the question sent by riddle to the network port and the challenge is done

> [!Caution]
> When attempting to listen to the port, riddle might still fail to connect. Check strace. The reason is probably your firewall. Make sure to allow a tcp connection to said port else this won't work. It doesn't matter if "you" haven't installed a firewall, there is one by default.


### Challenge 10: 

Name: "ESP"\
Hint: "Can you read my mind"\
What hex number am I thinking right now?

strace shows us an interesting behaviour of files. Riddle opens/creates a file "secret_number" and right after unlinks it with unlink() and writes the answer in there. Since there are no links on the file, it will be removed and become inaccesible however, in reality, it doesn't get completely removed until the last file descriptor (handled by riddle) is closed. In that mid-state is simply inaccessible by us.

One way we could probably access that deleted/not-deleted file would be through links. We can create the file beforehand ``touch secret_number`` and link it with another file. This way either through a hardlink or softlink we can access it's contents at any state.\
``ln secret_number secret_revealed``

Running riddle now and checking our secret_revealed file we get the password to input and complete the challenge.


### Challenge 11: 

Name: "ESP-2"\
Hint: "Can you read my mind?"

Running strace this seems very similar with the addition of an fstat call to the file. Repeating the same technique as before we get: 

"You are employing treacherous tricks. FAIL"

That's the reason fstat exists. It checks file's info and links. So another way would be to open the file in another program beforehand and read after riddle has added contents with our own file descriptor. Simply run [riddle-11.c](riddle-11.c) in another window before riddle, run riddle, press any key on the script to read the file and voila!.

Challenge is completed.

### Challenge 12: 

Name: "A delicate change"\
Hint: "Do what is required, nothing more, nothing less"


Strace shows a randomized file at /tmp/ being created followed by some memory manipulation with ftruncate and mmap. Also there is a 10 second timer were we can act so this will definately be a script.

Essentially, it creates a randomized file, resizes it to 4KB and then maps the page in virtual memory. Finally it provides an address 0x6f and a character which is trying to find. With these info we can open a file, truncate it accordinly and write a character however we need to find the randomized file and input the specific character. 

The files follow /tmp/riddle-XXXXX pattern and the character is given, so in the 10 second window, we can have our script [riddle-12.c](riddle-12.c) running and simply write the 5 digits of the randimized file and the required character.

Challenge Completed!

### Challenge 13: 

Name: "Bus error"
Hint: "Memquake! Don't lose the pages beneath your something something"

Again strace. Here a file is accessed, truncated and mapped into virtual memory with mmap and decreased to half its size right after. At this point riddle tries to access memory beyond the resized file and get killed with signal SIGBUS. The reading however happends after user input which is something we could use...

Since the memory-mapping is lost with the resize, we can't use mremap. We could however, re-allocate  the file back to the wanted size so the memory is mapped before any access.

``fallocate -o 32768 -l 16384 ".hello_there"``

Success!

### Challenge 14:

Name: "Are you the One?"\
Hint: "Are you 32767? If not, reincarnate!"\
I don't like my PID, get me another one. FAIL

This challenge seems fairly simple, riddle wants a specific PID. One thought would be to run a continuous fork of riddle until it gets it. [riddle-14.c](riddle-14.c) does just that if you are brave enough. There are 2 issues with this. First and most important one, it will put a huge strain on the cpu IF it can even reach that PID number.  Second issue is that riddle sees through this trick.

Another much better idea is to use a linux bahavioral feature.
Linux keeps the last pid generated by the kernel into a file. Changing that could "fool" the system and start riddle with the desired pid. The file is at /proc/sys/kernel/ns_last_pid so by running:

``sudo echo 32766 > /proc/sys/kernel/ns_last_pid; ./riddle``\
riddle gets 32767 pid and the mandatory segment is completed just like this guide.