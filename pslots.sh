#!/bin/bash
# Copyright (c) 2015 Dell Inc.  All Rights Reserved
#
# This code is released under LGPL license
#
# Display PCI slots for block and network devices
# Display bay/ID for SSD devices
function dcm()
{
    p=$1
    dcm=$2
    dev=0x${p:8:2}
    fun=0x${p:11:2}
    pfunc=$((dev * 8 + fun))
    
    while [ ! -z $dcm ] ;do
	port=${dcm:0:1}
	func=0x${dcm:1:1}
	pfi=0x${dcm:2:2}
	flag=0x${dcm:4:6}
	if [ $((func)) = $((pfunc)) ] ; then
	    echo "    p${port}_$((pfi))"
	fi
	dcm=${dcm:10}
    done
}
for Y in /sys/block/* /sys/class/net/* ; do
    RN=$(basename $Y)
    if [ ! -e $Y/device ] ; then
	continue
    fi
    RY=$(readlink -f $Y/device)
    echo ===== $RN
    echo "  Device: $RY"
    if [ -e $RY/physfn ] ; then
	RY=$(readlink -f $RY/physfn)
	echo "  PhysFn: $RY"
    fi
    if [ -e $Y/dev_port ] ; then
	DEVPORT=$(cat $Y/dev_port)
	echo "  DevPort: $DEVPORT"
    fi
    FLAG=""
    if [ -e $RY/vendor -a -e $RY/model ]; then
	echo -n "  Vendor: "
	cat $RY/vendor
	echo -n "  Model: "
	cat $RY/model
    fi
    while [ "x$RY" != "x/" ]; do
	P=$(basename $RY)
	if [ -e $RY/vpd ] ; then
	    DCM=$(lspci -vvvv -s $P | sed -n "s/.*DCM//p")
	    echo "  DCM: $DCM"
	    dcm $P $DCM
	fi
	if [ -e $RY/driver/module -a $((FLAG & 0x100)) == $((0x00)) ] ; then
	    RM=$(readlink -f $RY/driver/module)
	    MOD=$(basename $RM)
	    echo "  Driver: $MOD"
	    FLAG=$((FLAG | 0x100))
	fi
	# Only handle PCI devices
	if [ ! -e $RY/config ] ; then
	    RY=$(dirname $RY)
	    continue
	fi
	BUS=${P:5:2}
	BUSDEV=${P:5:5}
	if [ -e /usr/sbin/biosdecode ] ; then
	    biosdecode | grep "$BUSDEV.*slot" | sed -n "s/^\s*/  /p"
	fi
	CLS=$(cat $RY/class)
	if [ $((CLS & 0xF0000)) == $((0x10000)) ] ; then
	    # Storage device, get bay mapping
	    if [ -e /dev/ipmi -o -e /dev/ipmi0 ] ; then
                MAP=$(ipmitool raw 0x30 0xd5 0x01 0x07 0x06 0x00 0x00 0x00 0x$BUS 0x00 2> /dev/null)
                if [ "$MAP" != "" ] ; then
                    BAY=$(echo $MAP | cut -f8 -d' ')
                    SLOT=$(echo $MAP | cut -f9 -d' ')
                    if [ $BAY != "fe" ] ; then  
                        echo "  $P SSD bay:$BAY slot:$SLOT"
                    fi
                fi
	    fi
	fi
	if [ -e $RY/label -a $((FLAG & 0x1)) == $((0x00)) ] ; then
	    echo -n "  $P Label: "
	    cat $RY/label
	    FLAG=$((FLAG | 0x1))
	fi
	if [ -e $RY/acpi_index -a $((FLAG & 0x2)) == $((0x00)) ] ; then
	    echo -n "  $P ACPI Index: "
	    cat $RY/acpi_index
	    FLAG=$((FLAG | 0x2))
	fi
	if [ -e $RY/index -a $((FLAG & 0x4)) == $((0x00)) ] ; then
	    echo -n "  $P SMBIOS Index: "
	    cat $RY/index
	    FLAG=$((FLAG | 0x4))
	fi
	RY=$(dirname $RY)
    done
done

