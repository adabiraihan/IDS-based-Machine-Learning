"""
A module with various data structures used for interacting with and querying
the properties and status of Zeek packages.
"""

import os
import re

from .uservar import UserVar
from ._util import find_sentence_end

#: The name of files used by packages to store their metadata.
METADATA_FILENAME = 'zkg.meta'
LEGACY_METADATA_FILENAME = 'bro-pkg.meta'

TRACKING_METHOD_VERSION = 'version'
TRACKING_METHOD_BRANCH = 'branch'
TRACKING_METHOD_COMMIT = 'commit'

PLUGIN_MAGIC_FILE = '__bro_plugin__'
PLUGIN_MAGIC_FILE_DISABLED = '__bro_plugin__.disabled'

def name_from_path(path):
    """Returns the name of a package given a path to its git repository."""
    return canonical_url(path).split('/')[-1]


def canonical_url(path):
    """Returns the url of a package given a path to its git repo."""
    url = path.rstrip('/')

    if url.startswith('.') or url.startswith('/'):
        url = os.path.realpath(url)

    return url


def is_valid_name(name):
    """Returns True if name is a valid package name, else False."""
    if name != name.strip():
        # Reject names with leading/trailing whitespace
        return False

    if name in ("package", "packages"):
        return False

    return True


def aliases(metadata_dict):
    """Return a list of package aliases found in metadata's 'aliases' field."""
    if 'aliases' not in metadata_dict:
        return []

    return re.split(',\s*|\s+', metadata_dict['aliases'])


def tags(metadata_dict):
    """Return a list of tag strings found in the metadata's 'tags' field."""
    if 'tags' not in metadata_dict:
        return []

    return re.split(',\s*', metadata_dict['tags'])


def short_description(metadata_dict):
    """Returns the first sentence of the metadata's 'desciption' field."""
    if 'description' not in metadata_dict:
        return ''

    description = metadata_dict['description']
    lines = description.split('\n')
    rval = ''

    for line in lines:
        line = line.lstrip()
        rval += ' '
        period_idx = find_sentence_end(line)

        if period_idx == -1:
            rval += line
        else:
            rval += line[:period_idx + 1]
            break

    return rval.lstrip()


def user_vars(metadata_dict):
    """Returns a list of (str, str, str) from metadata's 'user_vars' field.

    Each entry in the returned list is a the name of a variable, its value,
    and its description.

    If the 'user_vars' field is not present, an empty list is returned.  If it
    is malformed, then None is returned.
    """
    uvars = UserVar.parse_dict(metadata_dict)

    if uvars is None:
        return None

    return [(uvar.name(), uvar.val(), uvar.desc()) for uvar in uvars]


def dependencies(metadata_dict, field='depends'):
    """Returns a dictionary of (str, str) based on metadata's dependency field.

    The keys indicate the name of a package (shorthand name or full git URL).
    The names 'zeek' or 'zkg' may also be keys that indicate a dependency on a
    particular Zeek or zkg version.

    The values indicate a semantic version requirement.

    If the dependency field is malformed (e.g. number of keys not equal to
    number of values), then None is returned.
    """
    if field not in metadata_dict:
        return dict()

    rval = dict()
    depends = metadata_dict[field]
    parts = depends.split()
    keys = parts[::2]
    values = parts[1::2]

    if len(keys) != len(values):
        return None

    for i, k in enumerate(keys):
        if i < len(values):
            rval[k] = values[i]

    return rval


class InstalledPackage(object):
    """An installed package and its current status.

    Attributes:
        package (:class:`Package`): the installed package

        status (:class:`PackageStatus`): the status of the installed package
    """

    def __init__(self, package, status):
        self.package = package
        self.status = status

    def __lt__(self, other):
        return str(self.package) < str(other.package)


class PackageStatus(object):
    """The status of an installed package.

    This class contains properties of a package related to how the package
    manager will operate on it.

    Attributes:
        is_loaded (bool): whether a package is marked as "loaded".

        is_pinned (bool): whether a package is allowed to be upgraded.

        is_outdated (bool): whether a newer version of the package exists.

        tracking_method (str): either "branch", "version", or "commit" to
            indicate (respectively) whether package upgrades should stick to a
            git branch, use git version tags, or do nothing because the
            package is to always use a specific git commit hash.

        current_version (str): the current version of the installed
            package, which is either a git branch name or a git version tag.

        current_hash (str): the git sha1 hash associated with installed
            package's current version/commit.
    """

    def __init__(self, is_loaded=False, is_pinned=False, is_outdated=False,
                 tracking_method=None, current_version=None, current_hash=None):
        self.is_loaded = is_loaded
        self.is_pinned = is_pinned
        self.is_outdated = is_outdated
        self.tracking_method = tracking_method
        self.current_version = current_version
        self.current_hash = current_hash


class PackageInfo(object):
    """Contains information on an arbitrary package.

    If the package is installed, then its status is also available.

    Attributes:
        package (:class:`Package`): the relevant Zeek package

        status (:class:`PackageStatus`): this attribute is set for installed
            packages

        metadata (dict of str -> str): the contents of the package's
            :file:`zkg.meta` or :file:`bro-pkg.meta`

        versions (list of str): a list of the package's availabe git version
            tags

        metadata_version: the package version that the metadata is from

        version_type: either 'version', 'branch', or 'commit' to
            indicate whether the package info/metadata was taken from a release
            version tag, a branch, or a specific commit hash.

        invalid_reason (str): this attribute is set when there is a problem
            with gathering package information and explains what went wrong.

        metadata_file: the absolute path to the :file:`zkg.meta` or
            :file:`bro-pkg.meta` for this package.  Use this if you'd like to
            parse the metadata yourself. May not be defined, in which case the
            value is None.
    """

    def __init__(self, package=None, status=None, metadata=None, versions=None,
                 metadata_version='', invalid_reason='', version_type='',
                 metadata_file=None, default_branch=None):
        self.package = package
        self.status = status
        self.metadata = {} if metadata is None else metadata
        self.versions = [] if versions is None else versions
        self.metadata_version = metadata_version
        self.version_type = version_type
        self.invalid_reason = invalid_reason
        self.metadata_file = metadata_file
        self.default_branch = default_branch

    def aliases(self):
        """Return a list of package name aliases.

        The canonical one is listed first.
        """
        return aliases(self.metadata)

    def tags(self):
        """Return a list of keyword tags associated with the package.

        This will be the contents of the package's `tags` field."""
        return tags(self.metadata)

    def short_description(self):
        """Return a short description of the package.

        This will be the first sentence of the package's 'description' field."""
        return short_description(self.metadata)

    def dependencies(self, field='depends'):
        """Returns a dictionary of dependency -> version strings.

        The keys indicate the name of a package (shorthand name or full git
        URL).  The names 'zeek' or 'zkg' may also be keys that indicate a
        dependency on a particular Zeek or zkg version.

        The values indicate a semantic version requirement.

        If the dependency field is malformed (e.g. number of keys not equal to
        number of values), then None is returned.
        """
        return dependencies(self.metadata, field)

    def user_vars(self):
        """Returns a list of user variables parsed from metadata's 'user_vars' field.

        If the 'user_vars' field is not present, an empty list is returned.  If
        it is malformed, then None is returned.

        Returns:
            list of zeekpkg.uservar.UserVar, or None on error
        """
        return UserVar.parse_dict(self.metadata)

    def best_version(self):
        """Returns the best/latest version of the package that is available.

        If the package has any git release tags, this returns the highest one,
        else it returns the default branch like 'main' or 'master'.
        """
        if self.versions:
            return self.versions[-1]

        return self.default_branch


class Package(object):
    """A Zeek package.

    This class contains properties of a package that are defined by the package
    git repository itself and the package source it came from.

    Attributes:
        git_url (str): the git URL which uniquely identifies where the
            Zeek package is located

        name (str): the canonical name of the package, which is always the
            last component of the git URL path

        source (str): the package source this package comes from, which
            may be empty if the package is not a part of a source (i.e. the user
            is referring directly to the package's git URL).

        directory (str): the directory within the package source where the
            :file:`zkg.index` containing this package is located.
            E.g. if the package source has a package named "foo" declared in
            :file:`alice/zkg.index`, then `dir` is equal to "alice".
            It may also be empty if the package is not part of a package source
            or if it's located in a top-level :file:`zkg.index` file.

        metadata (dict of str -> str): the contents of the package's
            :file:`zkg.meta` or :file:`bro-pkg.meta` file.  If the package has
            not been installed then this information may come from the last
            aggregation of the source's :file:`aggregate.meta` file (it may not
            be accurate/up-to-date).
    """

    def __init__(self, git_url, source='', directory='', metadata=None,
                 name=None, canonical=False):
        self.git_url = git_url
        self.source = source
        self.directory = directory
        self.metadata = {} if metadata is None else metadata
        self.name = name

        if not canonical:
            self.git_url = canonical_url(git_url)

            if not source and os.path.exists(git_url):
                # Ensures getting real path of relative directories.
                # e.g. canonical_url catches "./foo" but not "foo"
                self.git_url = os.path.realpath(self.git_url)

            self.name = name_from_path(git_url)

    def __str__(self):
        return self.qualified_name()

    def __repr__(self):
        return self.git_url

    def __lt__(self, other):
        return str(self) < str(other)

    def aliases(self):
        """Return a list of package name aliases.

        The canonical one is listed first.
        """
        return aliases(self.metadata)

    def tags(self):
        """Return a list of keyword tags associated with the package.

        This will be the contents of the package's `tags` field and may
        return results from the source's aggregated metadata if the package
        has not been installed yet."""
        return tags(self.metadata)

    def short_description(self):
        """Return a short description of the package.

        This will be the first sentence of the package's 'description' field
        and may return results from the source's aggregated metadata if the
        package has not been installed yet."""
        return short_description(self.metadata)

    def dependencies(self, field='depends'):
        """Returns a dictionary of dependency -> version strings.

        The keys indicate the name of a package (shorthand name or full git
        URL).  The names 'zeek' or 'zkg' may also be keys that indicate a
        dependency on a particular Zeek or zkg version.

        The values indicate a semantic version requirement.

        If the dependency field is malformed (e.g. number of keys not equal to
        number of values), then None is returned.
        """
        return dependencies(self.metadata, field)

    def user_vars(self):
        """Returns a list of (str, str, str) from metadata's 'user_vars' field.

        Each entry in the returned list is a the name of a variable, it's value,
        and its description.

        If the 'user_vars' field is not present, an empty list is returned.  If
        it is malformed, then None is returned.
        """
        return user_vars(self.metadata)

    def name_with_source_directory(self):
        """Return the package's name within its package source.

        E.g. for a package source with a package named "foo" in
        :file:`alice/zkg.index`, this method returns "alice/foo".
        If the package has no source or sub-directory within the source, then
        just the package name is returned.
        """
        if self.directory:
            return '{}/{}'.format(self.directory, self.name)

        return self.name

    def qualified_name(self):
        """Return the shortest name that qualifies/distinguishes the package.

        If the package is part of a source, then this returns
        "source_name/:meth:`name_with_source_directory()`", else the package's
        git URL is returned.
        """
        if self.source:
            return '{}/{}'.format(self.source,
                                  self.name_with_source_directory())

        return self.git_url

    def matches_path(self, path):
        """Return whether this package has a matching path/name.

        E.g for a package with :meth:`qualified_name()` of "zeek/alice/foo",
        the following inputs will match: "foo", "alice/foo", "zeek/alice/foo"
        """
        path_parts = path.split('/')

        if self.source:
            pkg_path = self.qualified_name()
            pkg_path_parts = pkg_path.split('/')

            for i, part in reversed(list(enumerate(path_parts))):
                ri = i - len(path_parts)

                if part != pkg_path_parts[ri]:
                    return False

            return True
        else:
            if len(path_parts) == 1 and path_parts[-1] == self.name:
                return True

            return path == self.git_url
