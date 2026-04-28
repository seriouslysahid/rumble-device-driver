Blacklist xpad

sudo rmmod xpad
echo "blacklist xpad" | sudo tee /etc/modprobe.d/blacklist-xpad.conf


For Normal Driver

cd driver
make clean && make
sudo insmod rumble.ko
sudo dmesg | grep rumble
ls /dev/rumble0
sudo chmod 666 /dev/rumble0

cd ..
python3 run.py

For Mouse Driver

make clean && make
sudo rmmod rumble
sudo insmod rumble.ko