## Operating Systems LAB - ECE NTUA Course 2021-2022

> [!NOTE]
> GitHub shows Batchfiles in this repo, it detects .cmd files from the generated VirtIO Device Modules, there aren't any usable batchfiles here.

This course has 3 big projects - assignments to be completed thorough the semester. All the work was done with teams of 2. 

### <div align="center"> 1st - Riddle </div>


A binary file filled with riddles to complete. Through its process you can get more accustomed to system calls and general operating system level operations (memory mapping, library calls etc.) while also learning the basics and thinking process for reverse engineering applications.

It consists of multiple challenges first 14 of which are mandatory for the assingment.
Detailed explanations and solutions are in its separate folder [here](Riddle/riddle_sol.md).


### <div align="center"> 2nd - Lunix Device Driver </div>

With this assingment we create a device driver from "scratch" (with  structures and guidelines on how to proceed provided) for wireless sensor network. The network consists of voltage and temperature readings with the base station containing wireless functionality and connects via USB on the Linux system where our driver is requested. For its functionallity, it is required to construct a character device driver to handle raw mesurments, process and export them in usable formats. Furthermore, its required to export these values to the user-space on different devices based on the datatype and the sensor it comes from along with support for multiple user access at the same time (possible through locks). Since, there are 3 readings available the template is as follows:
* /dev/lunix0-batt for battery
* /dev/lunix0-temp for temperature
* /dev/lunix0-light for light

TLDR of the driver: 

Data collection happens from [lunix-ldisc.c](/Lunix_Device_Driver/lunix-ldisc.c) Linux TTY Line discipline in "collaboration" with [lunix-protocol.c](/Lunix_Device_Driver/lunix-protocol.c) responsible for the communication protocol to report measurments for each connected device. It receives raw packet data from the Lunix line discipline and updated the corresponding sensor structure with the received values. These structures are then handled in memory from [lunix-sensors.c](/Lunix_Device_Driver/lunix-sensors.c) which manages all sensor buffers. Finally the buffers are exported in the user-space in their respective format creating the set of character devices. Here [lunix-attach.c](/Lunix_Device_Driver/lunix-attach.c) is required to attach the Linux Line Discipline to receive the data from a specific TTY and [lunix-chrdev.c](/Lunix_Device_Driver/lunix-chrdev.c) is required for each character device implementation to support all operations and requests and properly communicate with the user-space.\
All in all, a finalized module is generated and loaded in the kernel to communicate with all devices which become available in /dev/

* [lunix_dev_node.sh](/Lunix_Device_Driver/lunix_dev_nodes.sh) and [lunix-tcp.sh](/Lunix_Device_Driver/lunix-tcp.sh) are scripts to initialize connection and devices on the system. Note that connection endpoint has been redacted for privacy reasons.


### <div align="center"> 3rd - Virtio Crypto Chat </div>

This final assingment-project was to develop virtual hardware in QEMU-KVM. The target was to desing and inmpelent a virtual VirtIO cryptographic device which will be embeded in QEMU and allow processes inside the virtual machine to access real cryptography from the host device using paravirtualization. This access would happen through the cryptodev-linux device with calls to /dev/crypto. Driver usage would be through an encrypted chat terminal application over TCP with unix sockets.

Planning wise:
* First we create the chat app with normal sockets and unencrypted communication over TCP.

* Secondly, we create the virtual device for the VMs running QEMU where through para-virtualization we achieve communication between the real device and and the systems. This way any ioctl calls from the user in the QEMU guest machine will be  properly transfered to the hypervisor through virtIO and virtqueues which can call the real cryptographic device.

* Finally, we implement data encryption in the chat application using the crypto device driver and ioctl system calls to access it and encrypting the data before sending them over the network. (Decrypting them respectively when received)


### Chat files: 

* [Encrypted_chat](/Virtio_Crypto_Chat/encrypted_chat/):
    * [Server](/Virtio_Crypto_Chat/encrypted_chat/server.c)
    * [Client](/Virtio_Crypto_Chat/encrypted_chat/client.c)
    > The rest files in this folder are headers and the Makefile for compiling

* [Virtio CryptoDev Device](/Virtio_Crypto_Chat/virtio-cryptodev/):
    * [Guest (End-User)](/Virtio_Crypto_Chat/virtio-cryptodev/guest/):
        * [Crypto Character Device](/Virtio_Crypto_Chat/virtio-cryptodev/guest/crypto-chrdev.c) for device data handling and communication with user-space.
        * [Crypto Module](/Virtio_Crypto_Chat/virtio-cryptodev/guest/crypto-module.c): Kernel module invoked when virtio associated device is called.
        * [VirtIO Crypto Module](/Virtio_Crypto_Chat/virtio-cryptodev/guest/virtio_crypto.mod.c): VirtIO Kernel module

        * **Tests**:
            * [Crypto Test](/Virtio_Crypto_Chat/virtio-cryptodev/guest/test_crypto.c): Single encrypt/decrypt of random data
            * [Crypto Fork Test](/Virtio_Crypto_Chat/virtio-cryptodev/guest/test_fork_crypto.c): Forked encrypt/decrypt of random data
        > Headers, Makefiles and Bash scripts are available the directory but not listed here.
        > .o are compiled object files, .mod are compiled modules, .ko are compiled kernel modules.objects

    * [VirtIO QEMU Driver](/Virtio_Crypto_Chat/virtio-cryptodev/qemu_driver/):
        * [VirtIO](/Virtio_Crypto_Chat/virtio-cryptodev/qemu_driver/virtio-cryptodev.c)
