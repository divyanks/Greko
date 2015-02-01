#!/bin/bash

ret=0;
function check_syntax {

	if [ -z $1 ]; then
		echo "Invalid syntax "
		echo " ./ebdr_prepare.sh <mahine_type>"
		echo "machine_type = server | client | both"
		ret=1;
	else
		machine_type=$1
	fi
}

#########################################################
# check output of previous command, if not zero then fail
#########################################################
function status
{
	if [ $1 -ne 0 ]; then
		echo "Error: Failed in previous command execution "
		ret=1;
	fi

}


#######################################################
# Check if any ebdr daemon is running, if running then 
# kill it
#######################################################
function kill_ebdr {
	echo "Killing already running ebdr daemon";
	ebdrds_process=`ps -ef | grep ebdrds | grep -v "grep" | grep -v "gdb" | awk '{print $2}'`;
	if [ ! -z $ebdrds_process ]; then
		kill -9 $ebdrds_process;
		status $?;
	fi

	ebdrdc_process=`ps -ef | grep ebdrdc | grep -v "grep" | grep -v "gdb" | awk '{print $2}'`;
	if [ ! -z $ebdrdc_process ]; then
		kill -9 $ebdrdc_process;
		status $?;
	fi
	
	ebdrds_process=`ps -ef | grep ebdrds | grep -v "grep" | awk '{print $2}'`;
	if [ ! -z $ebdrds_process ]; then
		kill -9 $ebdrds_process;
		status $?;
	fi
	
	ebdrdc_process=`ps -ef | grep ebdrdc | grep -v "grep" | awk '{print $2}'`;
	if [ ! -z $ebdrdc_process ]; then
		kill -9 $ebdrdc_process;
		status $?;
	fi

}

########################################################
# Re-insert ebdr kernel module
########################################################
function reinsert_ebdr {
    
    # Removing ebdr module if existing
    lsmod_out=$(lsmod | grep ebdr); 
    if [ ! -z "$lsmod_out" ]; then
        echo "Removing existing ebdr module";
        rmmod ebdr.ko;
    	status $?;
   fi

    # Adding ebdr module if does not exist already
    lsmod_out=$(lsmod | grep ebdr);
    if [ -z "$lsmod_out" ]; then
        echo "Inserting new ebdr module";
        insmod ../../kernel/ebdr_linux/ebdr.ko;
		status $?;
    fi
}

########################################################
# Clean database
########################################################

function drop_db {

	db_name=`mysql -u root --password=root123 --execute='show databases' | grep "$1"`
	if [ ! -z "$db_name" ]; then
	   	mysql -u root --password=root123 --execute="drop database $db_name";
		status $?	
	fi

}

function clean_db {
	echo "Cleaning Database..."
    mysqld_status=`service mysqld status| grep "run"`;
    if [ -z "$mysqld_status" ]; then
        service mysqld start;
		status $?
    fi

	if [ "$1" == "both" ]; then
		drop_db ebdrdbs;
		drop_db ebdrdbc;
	elif [ "$1" == "server" ]; then
		drop_db ebdrdbs;
	elif [ "$1" == "client" ]; then
		drop_db ebdrdbc;
	fi



}

####################################################
# Print final result based on ret value
###################################################
function print_result {

	echo "-----------------------------------------------------"
	if [ $ret -ne 0 ]; then
		echo "Failed: Ebdr pre condition are not applied";
	else
		echo "Success: Ebdr pre conditions are applied"
	fi
	echo "----------------------------------------------------"

}

################################################
# Unmount existing ebdr
# right now it ummount only ebdr1 and ebdr2 
################################################

function unmount_ebdr {

	echo "Unmount existing ebdr "
	ebdr1_mount=`mount | grep ebdr1`;
	if [ ! -z "$ebdr1_mount" ]; then
		umount "/dev/ebdr1";
		status $?;
	fi

	ebdr2_mount=`mount | grep ebdr2`;
	if [ ! -z "$ebdr2_mount" ]; then
		umount "/dev/ebdr2";
		status $?;
	fi

	
}

###############
# main start here
###################
check_syntax $1
if [ $ret != 1 ]; then 
	kill_ebdr
	if [ "$machine_type" == "server" ] || [ "$machine_type" == "both" ]; then
		echo "Provide server or both"
		unmount_ebdr
	fi
	reinsert_ebdr
	clean_db $machine_type;
	print_result;
fi
