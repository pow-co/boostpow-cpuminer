FROM conanio/gcc10:latest

WORKDIR /home/conan/


COPY . /home/conan/boostminer
WORKDIR /home/conan/boostminer
RUN sudo chmod -R 777 .
RUN conan config set general.revisions_enabled=True
RUN conan profile new default --detect
RUN conan profile update settings.compiler.libcxx=libstdc++11 default
RUN conan remote add proofofwork https://conan.pow.co/artifactory/api/conan/conan

RUN conan install . -r=proofofwork
RUN conan build .

CMD ./bin/BoostMiner