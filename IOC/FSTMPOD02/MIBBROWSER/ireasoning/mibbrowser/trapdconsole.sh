#!/bin/sh
DIRNAME=`dirname $0`
BROWSER_HOME=$DIRNAME
java -Xmx384m -DisTrapdConsole=true  -Duser.country=US -Duser.language=en -jar $BROWSER_HOME/lib/browser.jar $*
