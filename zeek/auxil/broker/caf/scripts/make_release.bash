#!/bin/bash

# exit on first error
set -e

usage_string="\
Usage: $0 [--rc=NUM] VERSION
"

function usage() {
  echo "$usage_string"
  exit
}

function raise_error() {
  echo "*** $1" 1>&2
  exit
}

function file_ending() {
  echo $(basename "$1" | awk -F '.' '{if (NF > 1) print $NF}')
}

function assert_exists() {
  while [ $# -gt 0 ]; do
    if [ -z "$(file_ending "$1")" ]; then
      # assume directory for paths without file ending
      if [ ! -d "$1" ] ; then
        raise_error "directory $1 missing"
      fi
    else
      if [ ! -f "$1" ]; then
        raise_error "file $1 missing"
      fi
    fi
    shift
  done
}

function assert_exists_not() {
  while [ $# -gt 0 ]; do
    if [ -e "$1" ]; then
        raise_error "file $1 already exists"
    fi
    shift
  done
}

function assert_git_status_clean() {
  # save current directory
  anchor="$PWD"
  cd "$1"
  # check for untracked files
  untracked_files=$(git ls-files --others --exclude-standard)
  if [ -n "$untracked_files" ]; then
    raise_error "$1 contains untracked files"
  fi
  # check Git status
  if [ -n "$(git status --porcelain)" ] ; then
    raise_error "$1 is not in a clean state (see git status)"
  fi
  # restore directory
  cd "$anchor"
}

function ask_permission() {
  yes_or_no=""
  while [ "$yes_or_no" != "n" ] && [ "$yes_or_no" != "y" ]; do
    echo ">>> $1"
    read yes_or_no
  done
  if [ "$yes_or_no" = "n" ]; then
    raise_error "aborted"
  fi
}

rc_version=""

# Parse user input.

while [[ "$1" =~ ^-- ]]; do
  case "$1" in
    --*=*) optarg=$(echo "$1" | sed 's/[-_a-zA-Z0-9]*=//') ;;
  esac
  case "$1" in
    --rc=*)
      rc_version=$optarg
      ;;
    *)
      raise_error "unrecognized CLI option"
      ;;
  esac
  shift
done

if [ $# -ne 1  ] ; then
  usage
fi

echo "\

                         ____    _    _____
                        / ___|  / \  |  ___|    C++
                       | |     / _ \ | |_       Actor
                       | |___ / ___ \|  _|      Framework
                        \____/_/   \_|_|

This script expects to run at the root directory of a Git clone of CAF.
The current repository must be master. There must be no untracked file
and the working tree status must be equal to the current HEAD commit.
Further, the script expects a release_note.md file in the current directory
with the developer blog checked out one level above, i.e.:

\$HOME
├── .github-oauth-token

.
├── libcaf_io
├── blog_release_note.md [optional]
├── github_release_note.md

..
├── blog
│   ├── _posts

"

if [ $(git rev-parse --abbrev-ref HEAD) != "master" ]; then
  ask_permission "not in master branch, continue on branch [y] or abort [n]?"
fi

# assumed files
token_path="$HOME/.github-oauth-token"
github_msg="github_release_note.md"
config_hpp_path="libcaf_core/caf/config.hpp"

# assumed directories
blog_posts_path="../blog/_posts"

# check whether all expected files and directories exist
assert_exists "$token_path" "$config_hpp_path"

# check for a clean state
assert_exists_not .make-release-steps.bash
# assert_git_status_clean "."

# convert major.minor.patch version given as first argument into JJIIPP with:
#   JJ: two-digit (zero padded) major version
#   II: two-digit (zero padded) minor version
#   PP: two-digit (zero padded) patch version
# but omit leading zeros
version_str=$(echo "$1" | awk -F. '{ if ($1 > 0) printf("%d%02d%02d\n", $1, $2, $3); else printf("%02d%02d\n", $2, $3)  }')

# piping through AWK/printf makes sure 0.15 is expanded to 0.15.0
tag_version=$(echo "$1" | awk -F. '{ printf("%d.%d.%d", $1, $2, $3)  }')

if [ -n "$rc_version" ]; then
  tag_version="$tag_version-rc.$rc_version"
fi

# extract the release notes from the changelog
sed -n "/^## \[$tag_version\]*/,/^##[^#]/p" CHANGELOG.md | sed '$d' | awk '{ if (NR > 2) print $0}' > "$github_msg"

if [[ -s "$github_msg" ]] ; then
  echo ">>> please review the GitHub release notes as extracted from the changelog"
  cat "$github_msg"
  echo ; echo
  ask_permission "type [n] to abort or [y] to proceed"
else
  raise_error 'empty GitHub release notes'
fi

# add scaffold for release script
echo "\
#!/bin/bash
set -e
" > .make-release-steps.bash

echo ">>> patching config.hpp"
sed "s/#define CAF_VERSION [0-9]*/#define CAF_VERSION ${version_str}/g" < "$config_hpp_path" > .tmp_conf_hpp

# check whether the version actually changed
if cmp --silent .tmp_conf_hpp "$config_hpp_path" ; then
  rm .tmp_conf_hpp
  ask_permission "version already matches in config.hpp, continue pushing tag [y] or abort [n]?"
else
  mv .tmp_conf_hpp "$config_hpp_path"
  echo ; echo
  echo ">>> please review the diff reported by Git for patching config.hpp:"
  git diff
  echo ; echo
  ask_permission "type [n] to abort or [y] to proceed"
  echo "\
git commit -a -m \"Change version to $1\"
" >> .make-release-steps.bash
fi


token=$(cat "$token_path")
tag_descr=$(awk 1 ORS='\\r\\n' "$github_msg")
github_json=$(printf '{"tag_name": "%s","name": "%s","body": "%s","draft": false,"prerelease": false}' "$tag_version" "$tag_version" "$tag_descr")

# for returning to this directory after pushing blog
anchor="$PWD"

echo "\
git push
git tag $tag_version
git push origin --tags
curl --data '$github_json' -H 'Authorization: token $token' https://api.github.com/repos/actor-framework/actor-framework/releases
" >> .make-release-steps.bash

if [ -z "$rc_version" ]; then
  if which brew &>/dev/null ; then
    file_url="https://github.com/actor-framework/actor-framework/archive/$tag_version.tar.gz"
    echo "\
export HOMEBREW_GITHUB_API_TOKEN=\$(cat "$token_path")
brew bump-formula-pr --message=\"Update CAF to version $tag_version\" --url=\"$file_url\" caf
" >> .make-release-steps.bash
  fi
fi

echo ; echo
echo ">>> please review the final steps for releasing $tag_version "
cat .make-release-steps.bash
echo ; echo
ask_permission "type [y] to execute the steps above or [n] to abort"

chmod +x .make-release-steps.bash
./.make-release-steps.bash

echo ; echo
echo ">>> cleaning up"
rm "$github_msg" .make-release-steps.bash

echo ; echo
echo ">>> done"
