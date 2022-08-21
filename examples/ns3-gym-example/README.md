Currently developed with ns-3.35 (download from [here](https://www.nsnam.org/releases/)) and ns3-gym 1.0.1 (download from [here](https://apps.nsnam.org/app/ns3-gym/))

Additional commands used for successful build:
```
conda uninstall libgcc-ng
conda install libgcc-ng==7.5.0
conda install protobuf==3.6.1

mv ns-3.35 ns3-gym

py waf configure --enable-examples --enable-mpi --with-nsclick --with-openflow --with-nsc --force-planetlab --enable-sudo

py waf build

pip3 install --user src/opengym/model/ns3gym
```
