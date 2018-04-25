# Usage: source ./testing.sh
proj=.
src=./src
base=./build
host=localhost
#host=fawnlily.cs.engr.uky.edu
#host=fawnlily.cs.uky.edu
port=30000
key=38593

# alias real_smalld="./workingProgram/smalld $port $key"
alias smalld="$base/smalld $port $key"
alias smallSet="$base/smallSet $host $port $key"
alias smallGet="$base/smallGet $host $port $key"
alias smallDigest="$base/smallDigest $host $port $key"
alias smallRun="$base/smallRun $host $port $key"
