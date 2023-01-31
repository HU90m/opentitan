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
		echo "	help: prints this message."
		echo "	build: build the site and docs for prod"
		echo "	build-local: build the site and docs for a localhost server"
		echo "	serve: build and serve the site locally"
		exit 0
		;;
esac

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

# Get the project directory from the location of this script
proj_root="$PWD/$(dirname "$0")"

# Create the output directory
build_dir="$proj_root/build-docs"
mkdir -p "$build_dir"

# List of mdbook roots to build
mdbooks="
	hw
	util
	doc/books/project-governance
	doc/books/security
	doc/books/use-cases
	doc/guides/contributing
	doc/guides/getting-started
	doc/guides/rust-for-c-devs
"
#mdbooks+="sw"

for book_path in $mdbooks; do
	# Pull the site-url from the `book.toml` config
	site_url=$(sed -nr 's/site-url\s*=\s*"([^"]+)"/\1/p' "$book_path/book.toml")
	if [ -z "$site_url" ]; then
		echo "E: book '$book_path/book.toml' does not set a 'site-url'"
		exit 1;
	fi;

	mdbook build -d "$build_dir/$site_url" "$book_path"
done

# Build up the args
hugo_args=""
hugo_args+=" --source $proj_root/site/landing/"
hugo_args+=" --destination $build_dir/"

# If building or serving locally, set the baseURL to localhost
if [ "$command" = "build-local" ] || [ "$command" = "serve" ]; then
	hugo_args+=" --baseURL http://localhost:8000/"
fi

# Build website
# shellcheck disable=SC2086
hugo $hugo_args

# If serving, run the python HTTP server
if [ "$command" = "serve" ]; then
	python -m http.server -d "$build_dir"
fi
