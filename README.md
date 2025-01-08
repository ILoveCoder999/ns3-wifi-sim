# Latency map

## Install compiler
On Linux, g++ can be installed from the distro repository.
Then verify its installation:

```bash
g++ --version
```

## Install CMake
Install the latest version of [CMake](https://cmake.org/), downloading it from its website. This is recommended also for Linux since distro repositories can be very outdated. In this case download the .sh script (not the tar.gz file) and install it using the following command ([source](https://youtu.be/_yFPO1ofyF0?feature=shared)):

```bash
sudo sh nomefile.sh --prefix=/usr/local --exclude-subdir
```

On Windows, during the installation phase, make sure to check the "Add variable to path" (or similar) option, otherwise you will have to do it manually.

## Install Ninja (optional)
This section is not complete

```bash
apt get ninja-build
```

## Download and build
Download zip of NS3 from its website, uncompress it and move into directory (in VScode open a new window).

```bash
wget https://www.nsnam.org/release/ns-allinone-3.43.tar.bz2
tar xjf ns-allinone-3.43.tar.bz2
cd ns-allinone-3.43/ns-3.43/
```

Then configure and build and the library

```bash
./ns3 clean # clean previous build
./ns3 configure --build-profile=debug --enable-examples --enable-tests
./ns3 build
```
If you want to know the current configuration:

```bash
./ns3 show profile
```

## Configure VSCode
Install VSCode and add the following extensions:
- C++
- CMake language support (CMake Tools does not work well with ns3)

Open the folder containing only the ns3 library (e.g. *ns-allinone-3.43/ns-3.43/*) so that is the root folder in the explorer.

Project configuration and building via CMake should be performed via terminal as eplained previously.
Be sure to do this before going forward.

In order to launch and debug and executable, vscode uses launch configurations that are placed in the launch.json file in the *.vscode* folder.
These are selectable via the debug menu available in the lateral bar.
The one that is used is "(gdb) Launch from scratch".

A modification needs to be done so that the executable is found:
```json
"program": "${workspaceFolder}/build/${relativeFileDirname}/ns3-dev-${fileBasenameNoExtension}-${input:buildType}"// before
"program": "${workspaceFolder}/build/${relativeFileDirname}/ns3.43-${fileBasenameNoExtension}-${input:buildType}" // after
```
Now, with the source file of the executable open, the problem can be run.
When asked to select the *"build option"* (e.g. default, debug, etc...) pick the same that was used for project configuration.
As can be seen in the *program* string, this is part of the name of the executable to be run.

### Limitations
This build configuration uses as "preLaunchTask" to always build the executable before running it.
As shown in the *task.json* file, this invokes the "./ns3" commmand that re-builds all the modified source files in the project (not only the one you want to run).

It is not possibile to only run the executable without the debugger via the VSCode interface, because this is not supported by the C++ extension ([source](https://github.com/microsoft/vscode-cpptools/issues/3046)).
The extension functionality used by the debugger is specified in the *"type"* field in the debug configuration:

```bash
"type": "cppdbg"
```

### Environmental variables

To key-value enviromental variables append a new new dictionary containing the keys entries *name* and *value* to the *enviroment* list inside *launch.json*.
As an example, this is how you can configure the logging level for a component:

```json
{
	"name": "NS_LOG",
	"value": "UdpEchoClientApplication=level_all|prefix_func:UdpEchoServerApplication=level_all|prefix_func"
}
```

### Command line arguments

Just add the argument string to the *args* list in *launch.json*:

```json
"args": [
	"--PrintHelp",
	"--ns3::PointToPointNetDevice::DataRate=5Mbps",
	"--PrintAttributes=ns3::PointToPointChannel",
	"--PrintGroup=PointToPoint"
]
// these are example of functionalities
// options are not meant to be used together
```


## Traces

For reading pcap file it is possible to use either tcpdump or Wireshard.

```bash
tcpdump -nn -tt -r myfirst-0-0.pcap
```
	
PROBLEMI
- se rimuovo build dal launch.json succede un casino (solo con cmaketools)
- troppo lento a compilare (forse solo con cmaketools, che potrebbe limitare workers)


## Latency experiments

### Observations
- a the beginning of file more latency

## Cluster configuration
Use conda environments to install compiler required for ns-3 (the version on the cluster is outdated).
```bash
conda create --name ENV_NAME	# create new environment
conda env list 					# list environments
conda activate ENV_NAME 		# (ENV_NAME or ENV_PATH)
```

Install required packages:
```bash
conda install anaconda::gxx_linux-ppc64le 	# (currently it is the 11.2.0 version, conda forge 14.2 is bugged)
conda install python=3.11 					# later python versions currently do not work with numpy
conda install numpy
conda list									# list installed packages
```

It is also possibile to deactivate and/or remove an environment:
```bash
conda deactivate
conda remove -n ENV_NAME --all # remove all packages, i.e. delete enviroment
```

Check that the compiler that is being used is the one inside the enviroment and that it is the ```powerpc64le``` version:
```bash
env | grep CC
env | grep CXX
```

Configure and build ns3
```bash
./ns3 clear
rm -r cmake-cache # be sure it is removed, otherwise it will use the previous compiler
./ns3 configure --build-profile=optimized --enable-examples # configure ns-3
./ns3 build
```

Finally lauch the simulations
```bash
cd latency-test-v2/scripts
python -m submit_jobs BATCH_FILE BATCH_SIZE OUT_DIR # Use -1 for BATCH_SIZE to run all simulations
```


## TODO
- put step in configuration file
- different types of interferent (connected, not connected)
- metter a posto -1 e nan

## Simualazioni matteo
- latency-test-2.cc vs latency_test.cc ??? (latency-test-v2/latency-test.cc is the one uses on the server!!!)
- bug perchè l'interferente non viene messo ad altezza zero, ma ad altezza y

