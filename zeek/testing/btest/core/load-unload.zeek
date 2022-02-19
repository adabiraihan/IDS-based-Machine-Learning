# This tests the @unload directive
#
# @TEST-EXEC: zeek -b unload misc/loaded-scripts dontloadme > output
# @TEST-EXEC: btest-diff output
# @TEST-EXEC: grep dontloadme loaded_scripts.log && exit 1 || exit 0

@TEST-START-FILE unload.zeek
@unload dontloadme
@TEST-END-FILE

@TEST-START-FILE dontloadme.zeek
print "Loaded: dontloadme.zeek";
@TEST-END-FILE
