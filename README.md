# ESP32-C3 Dog Treat Dispencer

A dog treat dispencer for dogs that suffer from anxiety when their owner leaves home.
The dispencer can be controlled from afar by exposing the web server through the router's NAT or by using a service like ngrok.

Setup is:

* ESP32-C3 w/ mini LCD (LCD SDA pin 5, SCL pin 6)
* Micro Servo (connected to pin 2)
* The dispencer - yours will probably be better than mine (I don't have 3D printer so I used an old plastic bottle).

Code includes a machanism for uploading files to the SPIFFS, but first you will have to manually upload the index.html and admin.html files using mkspiffs & esptool or similar methods.
It also includes a mechanism to upload a compiled sketch using the web server (admin.html).

The story behind it:

My neighbour has a rescue dog that is barking non stop after she leaves the house
We thought of having some sort of contraption that will detect him getting close to the door or could be connected to a web panel that she can connect to from remote, and that will release treats for her dog.


There is, a very rough prototype that demonstrates the idea mechanically:



https://github.com/user-attachments/assets/4db81ff4-ab17-4e9f-b930-bc8b366d24ab

