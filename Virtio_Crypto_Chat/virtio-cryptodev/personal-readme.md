
# Disregard this file, it stayed for archival purposes



Here is a short guide <Personal Use>

virtio-cryptodev.c is to be implemented. as with ex 2, note pre-existing comments. 
we take ✨ inspiration ✨ from the Guest folder of virtio-cryptodev.
When done with the file we move it back to OKEANOS user / qemu-3.0.0 / hw / char / <here>   and we go to qemu dir 
"make && make install"  and this part is done

Starting utopia from now on should be with ./utopia.sh -device virtio-cryptodev-pci
( we just insert the virtio crypto device)


The guest folder has the rest what does that mean:

notice cryptodev-chrdev.c has to be implemented.

after that we "make" inside the guest folder

(we run  run-test.sh and pray) 
now we "insmod ./virtio_crypto.ko" // instead of insmod ./cryptodev-linux.ko
and run crypto_dev_nodes.sh same with sensors to create crypto devices.

lastly we need to take arg[] in our programs. instead of /dev/crypto  we need argv 
(filename = (argv[1] == NULL) ? "/dev/crypto" : argv[1];)  source: test_crypto.c


after that, since we have   ./utopia.sh -device .... 
and insmod .... and ./crypto_dev_nodes.sh  we simply run  ./server /dev/crypto<pick_your_poison> and ./client localhost 51003 /dev/crypto<pick_your_poison>
