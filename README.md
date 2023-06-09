# Remote-Procedure-Call

<img width="368" alt="Screenshot 2023-06-09 at 13 56 02" src="https://github.com/Hkfalkon/Remote-Procedure-Call/assets/74129398/3b1b6ba6-a45c-4e91-b12b-5123f8b62607">

1 Project Overview
RemoteProcedureCall RPC isacrucialtechnologyindistributedcomputingthatenablessoftwareapplications to communicate with each other seamlessly over a networkà It provides a way for a client to call a function on a remote server as if it were a local function callà This abstraction allows developers to build distributed systems and applications that span multiple machines and platformsà
In this project, you will be building a custom RPC system that allows computations to be split seamlessly between multiple computersà This system may differ from standard RPC systems, but the underlying principles of RPC will still applyà
Your RPC system must be written in Cà Submissions that do not compile and run on a Linux cloud VM, like the one you have been provided with, may receive zero marks. You must write your own RPC code, without using existing RPC librariesà
2 RPC System Architecture
YourtaskistodesignandcodeasimpleRemoteProcedureCall RPC systemusingaclient-serverarchitectureà The RPC system will be implemented in two les, called rpc.c and rpc.hà The resulting system can be linked to either a client or a serverà For marking, we will write our own clients and servers, and so you must stick to the proposed API carefullyà
Fortestingpurposes,youmayrunserverandclientprogramsonthesamemachine eàgà,yourVMà
3 Project Details
Your task is to design and code the RPC system described aboveà You will design the application layer protocol to useà A skeleton is provided which uses a simple application programming interface API à When we assess your submission, we will link our own testing code using the same RPC system APIà what you will be assessed on is rpc.c andanyothersupportinglescompiledinbyyourMakefileà
