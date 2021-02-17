sudo apt-get update
sudo apt-get install build-essential linux-headers-$(uname -r) git
sudo apt-get install libnuma-dev -y
#apt-get update

#Path which may be modified
#onvm-mos/core/src/Makefile.in needs to modify "ONVMPATH="
ONVMPATH=/users/weicuidi/openNetVM
ONVMMOSPATH=/users/weicuidi/onvm-mos

git clone git@github.com:chandaweia/openNetVM.git
git clone git@github.com:chandaweia/onvm-mos.git
cd $ONVMPATH
git checkout pub_sub_nf_tracker
echo export ONVM_HOME=$(pwd) >> ~/.bashrc

cd $ONVMMOSPATH
git checkout onvm-mos-new
cd $ONVMMOSPATH/drivers/dpdk-18.11
echo export RTE_SDK=$(pwd) >> ~/.bashrc
echo export RTE_TARGET=x86_64-native-linuxapp-gcc >> ~/.bashrc
echo export ONVM_NUM_HUGEPAGES=1024 >> ~/.bashrc
export ONVM_NIC_PCI=" 06:00.0 06:00.1 " >> ~/.bashrc
source ~/.bashrc

ifconfig enp6s0f0 down
ifconfig enp6s0f1 down


#install openNetVM
cd $ONVMPATH/scripts
./install.sh

#compile onvm
cd $ONVMPATH/onvm
make

#onvm-mos
cd $ONVMMOSPATH
./setup.sh --compile-dpdk
./setup.sh --run-dpdk
