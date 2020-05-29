apt-get update
sudo apt-get install build-essential linux-headers-$(uname -r) git
sudo apt-get install libnuma-dev
apt-get update

cd openNetVM/
echo export ONVM_HOME=$(pwd) >> ~/.bashrc

cd
git clone git@github.com:chandaweia/onvm-mos.git
git checkout onvm-mos-new

cd onvm-mos/

