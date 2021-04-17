#! perl -w write_unames.pl
#
# Description:
#   Write the names of the Unicode characters extracted from "UnicodeData-Latest.txt".
#   The names are written to the file "unames.str" which is included into the
#   resource file "getuname.rc" used to build getunames.dll.
#
# Author: John I. McConnell
#   23-Sep-2000     JohnMcCo    Created
#   27-Nov-2000     JohnMcCo    Converted most names to lowercase to make life easier
#                               for the localizers. Changed base string id to IDS_UNAME
#                               since uce uses IDS_UNICODE for a string resource (and
#                               includes getuname.h for some strange reason).
#

# Enforce the rules.
use strict;

# Read the UnicodeData-Latest.txt properties.
open(DATA, "UnicodeData-Latest.txt") or die "Could not open UnicodeData-Latest.txt $!";

# Open the new string file.
open(NAME, ">unames.str") or die "Could not open unames.str $!";

print NAME<<HEADER;
/*++

Copyright (c) 1997-2001,  Microsoft Corporation  All rights reserved.

Module Name:

    unames.str

Abstract:

    This file is generated by write_unames.pl. DO NOT EDIT.
    It contains the Unicode code value names used by getunames.dll.

Revision History:

    23-Sep-2000    JohnMcCo    Added support for Unicode 3.0
    27-Nov-2000    JohnMcCo    Changed names to title case and string ID to IDS_UNAME
    25-Jan-2001    JohnMcCo    Make each part of a hyphenated name uppercase.

--*/



STRINGTABLE DISCARDABLE
BEGIN
HEADER

# Copy names from UnicodeData-Latest to the string file.
while (<DATA>) {
    # Get the tab-separated fields.
    my ($cp, $name, @fields) = split /;/ or die "bad line in UnicodeData-Latest.txt";
    # If it is a control character, use the ISO name if it exists.
    if ($name =~ /<control>/ && length($fields[8]) > 0) {
        $name = $fields[8];
    }
    # If it's off the BMP or a range, ignore it.
    next if hex $cp > 0xffff || ($name =~ /^<.*,/);
    # Titlecase each word in the name except 'CJK'.
    my @words = map { /CJK/ ? $_ : ucfirst lc } split(' ', $name);
    # Titlecase each hyphen-separated word in the name.
    @words = split('-', $name);
    @words = map ucfirst, @words;
    $name = join('-', @words);
    # Print the string resource id value and name.
    print NAME 'IDS_UNAME + 0x', $cp, ' "', join(' ', @words), '"', "\n";
}
close(DATA);
print NAME "END\n";
close(NAME);

