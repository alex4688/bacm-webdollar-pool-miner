#!/bin/bash

#### COLOR SETTINGS ####
black=$(tput setaf 0 && tput bold)
red=$(tput setaf 1 && tput bold)
green=$(tput setaf 2 && tput bold)
yellow=$(tput setaf 3 && tput bold)
blue=$(tput setaf 4 && tput bold)
magenta=$(tput setaf 5 && tput bold)
cyan=$(tput setaf 6 && tput bold)
white=$(tput setaf 7 && tput bold)
blakbg=$(tput setab 0 && tput bold)
redbg=$(tput setab 1 && tput bold)
greenbg=$(tput setab 2 && tput bold)
yellowbg=$(tput setab 3 && tput bold)
bluebg=$(tput setab 4 && tput dim)
magentabg=$(tput setab 5 && tput bold)
cyanbg=$(tput setab 6 && tput bold)
whitebg=$(tput setab 7 && tput bold)
stand=$(tput sgr0)

### System dialog VARS
showinfo="$green[info]$stand"
showerror="$red[error]$stand"
showexecute="$yellow[running]$stand"
showok="$magenta[OK]$stand"
showdone="$blue[DONE]$stand"
showinput="$cyan[input]$stand"
showwarning="$red[warning]$stand"
showremove="$green[removing]$stand"
shownone="$magenta[none]$stand"
redhashtag="$redbg$white#$stand"
abortte="$cyan[abort to Exit]$stand"
showport="$yellow[PORT]$stand"
##

function set_cputhreads() {
    echo -e
    read -r -e -p "$showinput How many CPU_THREADS do you want to use? (your pc has ${green}$(nproc)$stand): " setcputhreads
    echo -e

    if [[ $setcputhreads == [nN] ]]; then
            echo -e "$showinfo OK..."

    elif [[ $setcputhreads =~ [[:digit:]] ]]; then

        if [[ $(grep "CPU_MAX:" $get_const_global | cut -d ',' -f1 | awk '{print $2}') == "$setcputhreads" ]]; then
            echo "$showinfo ${yellow}$(grep "CPU_MAX:" $get_const_global | cut -d ',' -f1)$stand is already set."
        else
            echo "$showexecute Setting terminal CPU_MAX to ${yellow}$setcputhreads$stand" && sed -i -- "s/CPU_MAX: $(grep "CPU_MAX:" $get_const_global | cut -d ',' -f1 | awk '{print $2}')/CPU_MAX:
cho "$showinfo Result: $(grep "CPU_MAX:" $get_const_global | cut -d ',' -f1)"
        fi

    elif [[ $setcputhreads == * ]]; then
        echo -e "$showerror Possible options are: digits or nN to abort." && set_cputhreads
    fi
}

#Script for getting the last master revision on linux systems
git reset --hard origin/master
git pull origin master
get_const_global="src/consts/const_global.js"
set_cputhreads
cd dist_bundle/argon2-cpu-miner/ && rm -rf ../CPU && mkdir ../CPU && mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make && cp libargon2.so libargon2.so.1 ../../../ && cp cpu-miner ../../CPU/ &&
2-cpu-miner && npm install

echo -e
echo -e "$showinfo BACM pool miner installed."
echo -e "$showinfo Execute the following commands if you want to start the BACM pool miner in screen."
echo -e "$showinfo Create new screen: cd bacm-miner && screen -S bacm-miner"
echo -e "$showinfo Start the miner: npm run commands"
echo -e
