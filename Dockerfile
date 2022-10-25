FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update

RUN apt-get -y install python3-pip build-essential git gcc-10 g++-10 cmake

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 20

RUN update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-10 20

RUN pip install conan

WORKDIR /home/conan/
#secp256k1
RUN git clone --depth 1 --branch v0.1 https://github.com/pow-co/secp256k1.git
WORKDIR  /home/conan/secp256k1
RUN conan create . proofofwork/stable

#data
WORKDIR /home/conan/
RUN git clone --depth 1 --branch master https://github.com/DanielKrawisz/data.git
WORKDIR /home/conan/data
RUN conan create . proofofwork/stable

#gigamonkey
WORKDIR /home/conan/
RUN git clone --depth 1 --branch master https://github.com/Gigamonkey-BSV/Gigamonkey.git
WORKDIR /home/conan/Gigamonkey
RUN conan create . proofofwork/stable

COPY . /home/conan/boostminer
WORKDIR /home/conan/boostminer
RUN sudo chmod -R 777 .
 
RUN conan install .
RUN conan build .

CMD ./bin/BoostMiner


