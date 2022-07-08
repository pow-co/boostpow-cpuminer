FROM conanio/gcc9:latest

WORKDIR /home/conan/
#secp256k1
RUN git clone --depth 1 --branch v0.1 https://github.com/pow-co/secp256k1.git
WORKDIR  /home/conan/secp256k1
RUN conan create . proofofwork/stable

#dataI
WORKDIR /home/conan/
RUN git clone --depth 1 --branch conan_change https://github.com/pow-co/data.git
WORKDIR  /home/conan/data
RUN conan create . proofofwork/stable

#gigamonkey
WORKDIR /home/conan/
RUN git clone --depth 1 --branch conan_change https://github.com/pow-co/Gigamonkey.git
WORKDIR  /home/conan/Gigamonkey
RUN conan create . proofofwork/stable

COPY . /home/conan/boostminer
WORKDIR /home/conan/boostminer
RUN sudo chmod -R 777 .
 
#RUN conan remote add proofofwork https://pow.jfrog.io/artifactory/api/conan/proofofwork

RUN conan install .
RUN conan build .

CMD ./bin/BoostMiner
