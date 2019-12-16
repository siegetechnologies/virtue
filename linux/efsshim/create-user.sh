#!/bin/bash
set -e
# set -x

function __usage() {
cat << EOF
"$(basename "$0") [OPTIONS] -- Launches resources within kubernetes.
where:
    -h          show this help text.
    -u          username of user to create
    -k          ssh key of the user
    -p          password of the user
EOF
}

# Get input parameters.
while getopts ":hu:k:p:" opt; do
  case "${opt}" in
    h )  __usage; exit 0               ;;
    u )  __USERNAME="${OPTARG}"  ;;
    k )  __AUTHKEY="${OPTARG}"  ;;
    p )  __PASSWORD="${OPTARG}"  ;;
    * )  echo "Option does not exist : $OPTARG" ; __usage ; exit 1 ;;
  esac    # --- end of case ---
done
shift $((OPTIND-1))

if [[ -z ${__USERNAME} ]]; then echo "Missing Required Option: -u username" ; exit 1 ; fi
if [[ -z ${__AUTHKEY} ]]; then echo "Missing Required Option: -k key" ; exit 1 ; fi
if [[ -z ${__PASSWORD} ]]; then echo "Missing Required Option: -p password" ; exit 1 ; fi

if id -u ${__USERNAME} ; then printf "User %s already exists" "${__USERNAME}" ; exit 1 ; fi


mkdir -p "/efs/${__USERNAME}"
useradd -d /efs/${__USERNAME} ${__USERNAME}
chown -R ${__USERNAME}:${__USERNAME} /efs/${__USERNAME}
echo ${__USERNAME}:${__PASSWORD} | chpasswd

mkdir "/var/ssh/${__USERNAME}/"
echo "${__AUTHKEY}" > "/var/ssh/${__USERNAME}/authorized_keys"
chown -R "${__USERNAME}:${__USERNAME}" "/var/ssh/${__USERNAME}/"
chmod 700 "/var/ssh/${__USERNAME}/"
chmod 600 "/var/ssh/${__USERNAME}/authorized_keys"


echo "should be Good 2 Go!"
