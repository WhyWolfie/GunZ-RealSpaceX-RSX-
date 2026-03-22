
This project initially aimed to add support for Gunz 2 maps (RS3) to Gunz 1 but later it changed to improving Gunz (features/quality/security).
In the mean time I joined legacy gamers team, but due to lack of devs the project was stalled. Thus we decided to share our work with the community in the hope that it will be useful and that the community will contribute back improving the source.


I'm releasing the full git source tree, the unpacked client with a compiled gunz binary (if any one wants to test it without having to compile the src) and the compiled server binaries.

This source is a cleaned up source with the following features:

* Support for RS3 maps
* Whole new file system, with new encryption and new compression algorithm (better compression & faster decompression)
* sha-512 db password hash (no plain pwd in the db)
* Other fixes

The client comes unpacked, with a test binary, which can read the unpacked files, and a packer utility.
To see the source after extracting it you'll have to 'git reset --hard' it.
Modified database is in the main source directory.

How to contribute

I intend to continue working on the source improving it and updating the thread when I have time. If some experienced dev with the 'open source spirit' wants to contribute FOR FREE and need some help (if I can help) feel free to ask me.

Users - can report bugs
Developers - can fix bugs / improve the source. I'll accept git patches, which will be merged on the git tree.
Level designers - If you want to build custom maps with special feature (physics/graphics effects), you can explain what you need and I'll work on adding these features, I'm also planing to create a 3ds plugin for exporting maps.


If you just want to try Gunz 2 maps you can setup a server with the provided binaries and database, run the game and create a 'mansion' room.
Notice that when creating your account in the db, in the password field you have to put your password sha-512.


This is my effort to bring Gunz to life again, but alone it will not work. I expect that the community will move this project forward.

Thanks to Keith and legacy team which worked on the source and provided support.

![gunz](https://raw.githubusercontent.com/WhyWolfie/GunZ-RealSpaceX-RSX-/refs/heads/main/Gunz1.jpg)
![gunz2](https://raw.githubusercontent.com/WhyWolfie/GunZ-RealSpaceX-RSX-/refs/heads/main/Gunz2.jpg)
![gunz3](https://raw.githubusercontent.com/WhyWolfie/GunZ-RealSpaceX-RSX-/refs/heads/main/Gunz3.jpg)
![gunz4](https://raw.githubusercontent.com/WhyWolfie/GunZ-RealSpaceX-RSX-/refs/heads/main/Gunz4.jpg)
![gunz5](https://raw.githubusercontent.com/WhyWolfie/GunZ-RealSpaceX-RSX-/refs/heads/main/Gunz5.jpg)
![gunz6](https://raw.githubusercontent.com/WhyWolfie/GunZ-RealSpaceX-RSX-/refs/heads/main/Gunz6.jpg)

Credits: **Grandao**
