#!/usr/bin/env sh

set -e

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
proj_root="$PWD/$(dirname $0)"

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

# Path to Hugo site root
hugo_path="site/landing"

hugo --source "$proj_root/site/landing/" --destination "$build_dir/"
