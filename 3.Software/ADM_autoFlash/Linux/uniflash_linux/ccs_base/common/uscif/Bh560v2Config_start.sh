#!/bin/bash
JAVAEXE=$1

if [ "${JAVAEXE}" == "" ]; then

	JAVAEXE=../../../eclipse/jre/bin/java

elif [ ! -e "${JAVAEXE}" ]; then

	JAVAEXE=../../../eclipse/jre/bin/java

fi


if [ ! -e "${JAVAEXE}" ]; then
	exit 1
fi


currentpath=$PWD
scriptpath=$(dirname $0)
cd $scriptpath

${JAVAEXE} -jar Bh560v2Config.jar

cd $currentpath
