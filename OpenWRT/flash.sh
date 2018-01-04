#!/usr/bin/expect -f

set dev "/dev/avrMultiTool"
set file "./bin/adm5120/openwrt-adm5120-router_le-br-6104k-squashfs-xmodem.bin"

system "stty 115200 ignbrk -brkint -icrnl -imaxbel -opost -onlcr -isig -icanon -iexten -echo -echoe -echok -echoctl -echoke < $dev"

spawn -open [ open $dev r+ ]
send_user "* Waiting for the prompt, please turn on the router\n"
expect "ADM5120 Boot:"
send_user "\n* Got prompt, waiting for the second prompt\n"
send "   "
expect "Please enter your key : "
send_user "\n* Got second prompt, uploading firmware $dev\n"
send "a"
close
system "sx -vv $file > $dev < $dev"
spawn -open [ open $dev r+ ]
expect "Please enter your key : "
send_user "\n* Got second prompt, booting\n"
send "c"

