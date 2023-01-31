#!/usr/bin/env bash

set -e

command="build"

case "$1" in
	"build"|"build-local"|"serve")
		command="$1"
		;;
	"help"|*)
		echo "USAGE: $0 [command]"
		echo ""
		echo "commands:"
		echo "	help         prints this message."
		echo "	build        build the site and docs for prod"
		echo "	build-local  build the site and docs for a localhost server"
		echo "	serve        build and serve the site locally"
		exit 0
		;;
esac

################
# DEPENDENCIES #
################

# Check for mdbook dep
if ! command -v mdbook >/dev/null; then
	echo "E: mdbook not found, please install from your package manager or with:" >&2
	echo "	$ cargo install mdbook" >&2
	exit 1
fi

# Check for hugo dep
if ! command -v hugo >/dev/null; then
	echo "E: hugo not found, please install from your package manager" >&2
	exit 1
fi

#################
# CONFIGURATION #
#################

# Get the project directory from the location of this script
proj_root="$PWD/$(dirname "$0")/.."

# Create the output directory
build_dir="$proj_root/build-docs"
mkdir -p "$build_dir"

# List of mdbook roots to build
mdbooks="
	doc/hardware
	doc/software
	doc/tools
	doc/project-governance
	doc/security
	doc/use-cases
	doc/guides/contributing
	doc/guides/getting-started
	doc/guides/rust-for-c-devs
"

# Apply custom config to insert the website theme
export MDBOOK_OUTPUT__HTML__THEME="$proj_root/site/docs/theme/"

# Build up Hugo arguments
hugo_args=""
hugo_args+=" --source $proj_root/site/landing/"
hugo_args+=" --destination $build_dir/"

# If building or serving locally, set base URLs to localhost
if [ "$command" = "build-local" ] || [ "$command" = "serve" ]; then
	hugo_args+=" --baseURL http://localhost:8000/"
	export HUGOxPARAMSxDOCS_URL="http://localhost:8000/doc"
	export URL_ROOT="http://localhost:8000/doc"
else
	export URL_ROOT="https://docs.opentitan.org"
fi

############
# BUILDING #
############

for book_path in $mdbooks; do
    mdbook build -d "$build_dir/$book_path" "$book_path"
done

# Build website
# shellcheck disable=SC2086
hugo $hugo_args

###########
# SERVING #
###########

# If serving, run the python HTTP server
if [ "$command" = "serve" ]; then
	python -m http.server -d "$build_dir"
fi
