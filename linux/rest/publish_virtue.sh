#!/bin/bash
set -e
# set -x
__DOCKERDIR="./docker"

function __usage() {
cat << EOF
"$(basename "$0") [OPTIONS] -- Launches resources within kubernetes.
where:
    -h          show this help text.
    -r          set the docker registry location for the app.
    -p          set the name of the registry repo.
    -t          (optional) tar.gz files to extract.
    -i          (optional) software to install via apt."
    -c          (optional) commands to run"
EOF
}

# Get input parameters.
while getopts ":hr:p:t:i:" opt; do
  case "${opt}" in

    h )  __usage; exit 0               ;;
    r )  __REGISTRY="${OPTARG}"  ;;
    p )  __REPO="${OPTARG}"  ;;
    t )  __TARGZ="${OPTARG}"  ;;
    i )  __INSTALL="${OPTARG}"  ;;
    c )  __POSTINSTALL="${OPTARG}"  ;;
    * )  echo "Option does not exist : $OPTARG" ; __usage ; exit 1 ;;

  esac    # --- end of case ---
done
shift $((OPTIND-1))

# Validate input parameters.
if [[ -z $__REGISTRY ]]; then echo "Missing Required Option: -r registry" ; exit 1 ; fi
if [[ -z $__REPO ]]; then echo "Missing Required Option: -v virtuerepo" ; exit 1 ; fi

# Log in to ECR
aws ecr get-login | bash

# Create custom virtue.dockerfile.
__EXISTSREPO="$(aws ecr describe-repositories | grep $__REPO | cut -d ':' -f2 | cut -d '"' -f2)"
if [[ -z $__EXISTSREPO ]] ; then
  aws ecr create-repository --repository-name $__REPO
fi

rsync --no-relative -r $__DOCKERDIR/ $__DOCKERDIR/$__REPO

if ! [[ -z $__INSTALL ]] ; then
  sed -i "s/#__INSTALL/RUN apt-get install -y $__INSTALL/g" $__DOCKERDIR/$__REPO/Dockerfile
fi


if ! [[ -z $__POSTINSTALL ]] ; then
	__PINSTALL=""
	while read -r line; do
		puftinstall = "${PINSTALL}
RUN ${line}"
		echo "... ${PINSTALL} ..."
	done <<< $__POSTINSTALL
  sed -i "s/#__POSTINSTALL/$__PINSTALL/g" $__DOCKERDIR/$__REPO/Dockerfile
fi

if ! [[ -z $__TARGZ ]] ; then
  cp -p $__TARGZ $__DOCKERDIR/$__REPO/app.tar.gz
  sed -i "s/#__TARGZ/ADD app.tar.gz \//g" $__DOCKERDIR/$__REPO/Dockerfile
fi

# Build the Virtue Image.
docker build -t $__REGISTRY/$__REPO $__DOCKERDIR/$__REPO/.
docker push $__REGISTRY/$__REPO:latest

# Clean up temporary files.
rm -rf app.tar.gz $__DOCKERDIR/$__REPO
