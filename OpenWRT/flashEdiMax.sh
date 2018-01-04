#!/usr/bin/expect -f

set dev "/dev/ttyUSB0"
set file "openwrt-adm5120-br-6104kp-squashfs-xmodem.bin"

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

