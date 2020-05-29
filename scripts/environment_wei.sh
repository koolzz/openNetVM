apt-get update
sudo apt-get install build-essential linux-headers-$(uname -r) git
sudo apt-get install libnuma-dev -y
apt-get update

cd /root/openNetVM
git checkout pub_sub_nf_tracker
echo export ONVM_HOME=$(pwd) >> ~/.bashrc

cd
git clone git@github.com:chandaweia/onvm-mos.git
git checkout onvm-mos-new
cd /root/onvm-mos/drivers/dpdk-18.11
echo export RTE_SDK=$(pwd) >> ~/.bashrc
echo export RTE_TARGET=x86_64-native-linuxapp-gcc >> ~/.bashrc
echo export ONVM_NUM_HUGEPAGES=1024 >> ~/.bashrc
export ONVM_NIC_PCI=" 06:00.0 06:00.1 " >> ~/.bashrc
source ~/.bashrc

ifconfig enp6s0f0 down
ifconfig enp6s0f1 down


#install openNetVM
cd /root/openNetVM/scripts
./install.sh

#compile onvm
cd /root/openNetVM/onvm
make

#onvm-mos
cd /root/onvm-mos
./setup.sh --compile-dpdk
