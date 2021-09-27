FROM conanio/gcc9:latest
 
COPY . /home/conan/boostminer
WORKDIR /home/conan/boostminer
RUN sudo chmod -R 777 .
 
RUN conan remote add proofofwork https://pow.jfrog.io/artifactory/api/conan/proofofwork

RUN conan install . -s compiler.libcxx=libstdc++11
RUN conan build .

CMD ./BoostMiner
