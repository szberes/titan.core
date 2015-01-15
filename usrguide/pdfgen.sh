###############################################################################
# Copyright (c) 2000-2014 Ericsson Telecom AB
# All rights reserved. This program and the accompanying materials
# are made available under the terms of the Eclipse Public License v1.0
# which accompanies this distribution, and is available at
# http://www.eclipse.org/legal/epl-v10.html
###############################################################################
scp *.doc esekilxxen1843.rnd.ericsson.se:/home/titanrt/jenkins/usrguide_pdf/
ssh esekilxxen1843.rnd.ericsson.se "make -C /home/titanrt/jenkins/usrguide_pdf && exit"
scp esekilxxen1843.rnd.ericsson.se:/home/titanrt/jenkins/usrguide_pdf/*.pdf .