# ocpjbod
ocpjbod is a SCSI enclosure management tool that controls some OCP
storage enclosure and storage server, like Open Vault (Knox),
Honey Badger, etc. Details about these hardware are available at
http://www.opencompute.org/wiki/Storage.

ocpjbod is made to be extendible, we welcome external contributions.

Discussions are conducted in https://www.facebook.com/groups/ocpjbod.

## Requirements
ocpjbod requires or works with
* Linux
* sg3_utils-libs (>= 1.28)
* sg3_utils-devel (>= 1.28)
* libcurl
* libcurl-devel
* json-c
* json-c-devel
* switchtec-user

## Building ocpjbod
1. Make sure gcc, make and required libraries are installed.

2. Run in ocpjbod directory:

    make all

## Installing ocpjbod
After successful make, run in ocpjbod directory:

    make install

## Example Commands
    ocpjbod list

    ocpjbod hdd --hdd-on 0 /dev/sg1

    ocpjbod sensor /dev/sg1
