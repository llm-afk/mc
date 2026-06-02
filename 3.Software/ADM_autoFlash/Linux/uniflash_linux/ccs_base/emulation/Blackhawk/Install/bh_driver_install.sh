#!/bin/bash

#===============================================================================
# Version:	v1.0
# Date: 	03/08/2011
#===============================================================================
#
#
# bh_driver_install.sh 
#		- perform device driver installation if user is root
#
#
#===============================================================================
#===============================================================================

################################################################################
#
#	BHFUNC_Print2Log()
#
#		Append $* to log file.
#

BHFUNC_Print2Log() {
	echo $* >> ${BH_LOGFILE}
}

################################################################################
#
#	BHFUNC_InstallRules()
#
#		INSTALL Script Commands for .rules
#

BHFUNC_InstallRules() {

	BHFUNC_Print2Log ""
	BHFUNC_Print2Log "RULES..."

	STARTDIR=`dirname $0`
	USER=`whoami`
	OPT=$1
	UNINSTALL=0
	bhUDEV_FILE="/etc/udev/rules.d/71-bh-permissions.rules"
	bhRULES_FILE="${STARTDIR}/71-bh-permissions.rules"


# For Redhat use the start_udev script
	RESTARTUDEV="/sbin/start_udev"

	# For others use "System V" service command
	if [ ! -e ${RESTARTUDEV} ]; then
		RESTARTUDEV="service udev restart"
	fi


	# Remove old rules file
	if [ -e ${bhUDEV_FILE} ]; then
		BHFUNC_Print2Log  "Uninstalling ${bhUDEV_FILE}"
		rm -f ${bhUDEV_FILE}
		if [ $? -ne 0 ]; then
			BHFUNC_Print2Log  "ERROR: failed to remove ${bhUDEV_FILE}"
			echo "ERROR: failed to remove ${bhUDEV_FILE}"
			exit 1
		fi
	fi


	# Check rules file is not missing
	if [ ! -e ${bhRULES_FILE} ]; then
		BHFUNC_Print2Log  "ERROR: the required component ${bhRULES_FILE} is missing from the installation"
		echo  "ERROR: the required component ${bhRULES_FILE} is missing from the installation"
		exit 1
	fi


	# Copy the new rules file and change its permissions
	cp ${bhRULES_FILE} ${bhUDEV_FILE}
	if [ $? -ne 0 ]; then
		BHFUNC_Print2Log  "ERROR: failed to copy ${bhRULES_FILE} to ${bhUDEV_FILE}"
		echo  "ERROR: failed to copy ${bhRULES_FILE} to ${bhUDEV_FILE}"
		exit 1
	fi


	# Change its permissions
	PERMISSIONS=644
	chmod ${PERMISSIONS} ${bhUDEV_FILE}
	if [ $? -ne 0 ]; then
		BHFUNC_Print2Log  "ERROR: failed to set the permissions for ${bhRULES_FILE} to ${PERMISSIONS}"
		echo  "ERROR: failed to set the permissions for ${bhRULES_FILE} to ${PERMISSIONS}"
		exit 1
	fi


	# All done
	BHFUNC_Print2Log  "bh usb560 installation completed successfully.  Some versions of Linux"
	BHFUNC_Print2Log  "require a reboot in order for the driver to function properly.  For other"
	BHFUNC_Print2Log  "versions restarting udev is sufficient.  Restarting udev now ... "
	echo "bh usb560 installation completed successfully.  Some versions of Linux"
	echo "require a reboot in order for the driver to function properly.  For other"
	echo "versions restarting udev is sufficient.  Restarting udev now ... "


	# Restart udev
	${RESTARTUDEV}
	if [ $? -ne 0 ]; then
		BHFUNC_Print2Log  "ERROR: failed to restart udev, reboot required"
		echo "ERROR: failed to restart udev, reboot required"
		exit 1
	fi

}


################################################################################
#
#	BHFUNC_InstallKO()
#
#		INSTALL Script Commands for Kernel Objects
#

BHFUNC_InstallKO() {

	BHFUNC_Print2Log ""
	BHFUNC_Print2Log "KERNEL OBJECTS..."

	cd ../Bh560v2
	BHFUNC_Print2Log "- from: [`pwd`]"
	
	BH_UNAME=`uname -r`
	BHFUNC_Print2Log "UNAME: [${BH_UNAME}]"

	BHFUNC_Print2Log "Installing Blackhawk XDS560v2 USB Kernel Object for Linux..."
	
	BH_KODIR="linux.ko"

	if [ -d "${BH_KODIR}/${BH_UNAME}" ]; then

		BHFUNC_Print2Log "${BH_KODIR}/${BH_UNAME}"
		BHFUNC_Print2Log "insmod ${BH_KODIR}/${BH_UNAME}/h_dtc.ko"
		/sbin/insmod ${BH_KODIR}/${BH_UNAME}/h_dtc.ko

	else

		BHFUNC_Print2Log "${BH_KODIR}"
		BHFUNC_Print2Log "bhWARNING: Using Generic KO driver"
		BHFUNC_Print2Log "${BH_KODIR}/2.6.32-21-generic"
		BHFUNC_Print2Log "insmod ${BH_KODIR}/2.6.32-21-generic/h_dtc.ko"
		/sbin/insmod ${BH_KODIR}/2.6.32-21-generic/h_dtc.ko

	fi

	cd ${BH_CURRENTDIR}
}

################################################################################
#
#	BHFUNC_Install()
#
#		INSTALL Script Commands
#

BHFUNC_Install() {

	BHFUNC_Print2Log ""
	BHFUNC_Print2Log "Running Install Script Commands..."

	#BHFUNC_InstallKO

	BHFUNC_InstallRules

}

################################################################################
#
#	BHFUNC_UninstallRules()
#
#		UNINSTALL Script Commands for .rules
#

BHFUNC_UninstallRules() {

	BHFUNC_Print2Log ""
	BHFUNC_Print2Log "RULES..."

	STARTDIR=`dirname $0`
	USER=`whoami`
	OPT=$1
	UNINSTALL=0
	bhUDEV_FILE="/etc/udev/rules.d/71-bh-permissions.rules"
	bhRULES_FILE="${STARTDIR}/71-bh-permissions.rules"


# For Redhat use the start_udev script
	RESTARTUDEV="/sbin/start_udev"

	# For others use "System V" service command
	if [ ! -e ${RESTARTUDEV} ]; then
		RESTARTUDEV="service udev restart"
	fi


	# Remove old rules file
	if [ -e ${bhUDEV_FILE} ]; then
		BHFUNC_Print2Log  "Uninstalling ${bhUDEV_FILE}"
		rm -f ${bhUDEV_FILE}
		if [ $? -ne 0 ]; then
			BHFUNC_Print2Log  "ERROR: failed to remove ${bhUDEV_FILE}"
			echo "ERROR: failed to remove ${bhUDEV_FILE}"
			exit 1
		fi
	fi
}

################################################################################
#
#	BHFUNC_UninstallKO()
#
#		UNINSTALL Script Commands - Kernel Object
#

BHFUNC_UninstallKO() {

	BHFUNC_Print2Log ""
	BHFUNC_Print2Log "KERNEL OBJECTS..."

	cd ../Bh560v2
	BHFUNC_Print2Log "- from: [`pwd`]"
	
	BH_UNAME=`uname -r`
	BHFUNC_Print2Log "UNAME: [${BH_UNAME}]"

	BHFUNC_Print2Log "UNinstalling Blackhawk XDS560v2 USB Kernel Object for Linux..."
	
	BH_KODIR="linux.ko"

	if [ -d "${BH_KODIR}/${BH_UNAME}" ]; then

		BHFUNC_Print2Log "${BH_KODIR}/${BH_UNAME}"
		BHFUNC_Print2Log "rmmod ${BH_KODIR}/${BH_UNAME}/h_dtc.ko"
		/sbin/rmmod ${BH_KODIR}/${BH_UNAME}/h_dtc.ko

	else

		BHFUNC_Print2Log "${BH_KODIR}"
		BHFUNC_Print2Log "bhWARNING: Using Generic KO driver"
		BHFUNC_Print2Log "${BH_KODIR}/2.6.32-21-generic"
		BHFUNC_Print2Log "rmmod ${BH_KODIR}/2.6.32-21-generic/h_dtc.ko"
		/sbin/rmsmod ${BH_KODIR}/2.6.32-21-generic/h_dtc.ko

	fi

	cd ${BH_CURRENTDIR}
}

################################################################################
#
#	BHFUNC_Uninstall()
#
#		UNINSTALL Script Commands
#

BHFUNC_Uninstall() {

	BHFUNC_Print2Log ""
	BHFUNC_Print2Log "Running Uninstall Script Commands..."

	#BHFUNC_UninstallKO

	BHFUNC_UninstallRules

}


################################################################################
################################################################################
#
#	main()
#
################################################################################

BH_CURRENTDIR=`pwd`
BH_SubFolder=""
BH_LOGFILE="${BH_CURRENTDIR}/bh_emulation_install.log"
BH_TestFile="/common/uscif/uscifv2.out"

# check that user is root
BH_WHOAMI=`whoami`
if [ "${BH_WHOAMI}" != "root" ]; then
	echo "bhERROR: this script (bh_emulation_install.sh) must be run as root."
	exit 1
fi


# start the log
BHFUNC_Print2Log ""
BHFUNC_Print2Log "++++++++++++++++++++++++++++++++++++++++++++++++++++++"
BHFUNC_Print2Log "Script Started. `date`"
BHFUNC_Print2Log $0 $*
BHFUNC_Print2Log "User: $BH_WHOAMI"


# find the base install directory
#BHFUNC_FindInstallDir

# process un/install
BHFUNC_Print2Log ""
BHFUNC_Print2Log "Processing command line option(s)..."

if [ "$1" == "--install" ]; then
	BHFUNC_Print2Log "INSTALL"
	BHFUNC_Install
elif [ "$1" == "--uninstall" ];then 
	BHFUNC_Print2Log "UNINSTALL"
	BHFUNC_Uninstall
else
	BHFUNC_Print2Log "BHWARNING: Empty command line.  Now exiting..."
	echo "bhWARNING: Empty command line.  Now exiting..."
fi

BHFUNC_Print2Log ""
BHFUNC_Print2Log "Script Finished. `date`"

exit 0
