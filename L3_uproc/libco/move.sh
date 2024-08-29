
make
sudo rm /usr/local/lib/libco-32.so
sudo rm /usr/local/lib/libco-64.so
sudo cp libco-32.so /usr/local/lib 
sudo cp libco-64.so /usr/local/lib
sudo ldconfig