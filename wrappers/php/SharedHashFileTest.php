<?php

/*
 * ============================================================================
 * Copyright (c) 2014 Hardy-Francis Enterprises Inc.
 * This file is part of SharedHashFile.
 *
 * SharedHashFile is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * SharedHashFile is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see www.gnu.org/licenses/.
 * ----------------------------------------------------------------------------
 * To use SharedHashFile in a closed-source product, commercial licenses are
 * available; email office [@] sharedhashfile [.] com for more information.
 * ============================================================================
 */

class SharedHashFileTest extends PHPUnit_Framework_TestCase
{
    public function testSharedHashFile()
    {
        $testShfFolder = "/dev/shm";
        $testShfName   = "test-php-" . getmypid();
        $shfUidNone    = 4294967295;

        print "PHP: calling new SharedHashFile()\n";
        $shf = new SharedHashFile();
//        $shf2 = new SharedHashFile();

        print "PHP: calling ->attach()\n";
        $shf->attach($testShfFolder, $testShfName); // todo: add final , 1 parameter

        $key = "foo";
        print "PHP: calling ->putKeyVal()\n";         $shf->putKeyVal($key, "bar");
        print "PHP: calling ->getKeyVal()\n"; $val1 = $shf->getKeyVal($key); print "PHP: got val='$val1' for key=$key\n";
//        print "PHP: calling ->delKeyVal()\n";         $shf->delKeyVal($key);

        print "PHP: calling ->putKeyValOver()\n";         $shf->putKeyValOver($key, "zoo");
        print "PHP: calling ->getKeyVal()\n"; $val2 = $shf->getKeyVal($key); print "PHP: got val='$val2' for key=$key\n";
        print "PHP: calling ->delKeyVal()\n";         $shf->delKeyVal($key);

		if (0)
		{
			$start = microtime(true);
			$count = 0;
			while ($count < 750000)
			{
				$count ++;
				$shf->putKeyVal("foo" . $count, $count);
			}
			$end = microtime(true);
			$elapsed = $end - $start;
			printf("- calling ->putKeyVal() very many times took %f seconds or %u calls per second\n", $elapsed, $count / $elapsed);

			$start = microtime(true);
			$count = 0;
			while ($count < 750000)
			{
				$count ++;
				$shf->putKeyValOver("foo" . $count, $count - 1);
			}
			$end = microtime(true);
			$elapsed = $end - $start;
			printf("- calling ->putKeyValOver() very many times took %f seconds or %u calls per second\n", $elapsed, $count / $elapsed);

			$start = microtime(true);
			$count = 0;
			while ($count < 750000)
			{
				$count ++;
				$val = $shf->getKeyVal("foo" . $count);
			}
			$end = microtime(true);
			$elapsed = $end - $start;
			printf("- calling ->getKeyVal() very many times took %f seconds or %u calls per second\n", $elapsed, $count / $elapsed);
		}

		$shf_thrash = new SharedHashFile(); $shf_thrash->attach($testShfFolder, $testShfName . "-thrash"); // todo: add final , 1 parameter
		$shf_rand   = new SharedHashFile(); $shf_rand->attach($testShfFolder, $testShfName . "-rand"  ); // todo: add final , 1 parameter

		print "PHP: calling ->putKeyVal()\n";             $shf_rand->putKeyVal($key, "bar");
#		print "PHP: calling ->putKeyValOver()\n";         $shf_thrash->putKeyValOver($key, "zoo");
		print "PHP: calling ->putKeyValOver()\n";         $shf_rand->putKeyValOver($key, "zoo");
		print "PHP: calling ->getKeyValOver()\n"; $val2 = $shf_rand->getKeyVal($key); print "PHP: got val='$val2' for key=$key\n";
#		print "PHP: calling ->delKeyVal()\n";             $shf_rand->delKeyVal($key);

		print "PHP: calling ->putKeyValOver()\n";         $shf_rand->putKeyValOver($key, "zoo!");
		print "PHP: calling ->getKeyValOver()\n"; $val3 = $shf_rand->getKeyVal($key); print "PHP: got val='$val3' for key=$key\n";

        $this->assertEquals(1, 1, 'my message');
        $this->assertEquals(1, 0, 'my message 2');
    }
}

# print "- calling helloworld()\n";
# helloworld();
#
# print "- calling new SharedHashFile()\n";
# $myc1 = new SharedHashFile();
# var_dump($myc1);
#
# print "- calling getDoomsday()\n";
# $myc1->getDoomsday();
#
# print "- calling new SharedHashFile()\n";
# $myc2 = new SharedHashFile();
# var_dump($myc2);
#
# print "- calling getDoomsday()\n";
# $myc2->getDoomsday();
#
# print "- calling getDoomsday()\n";
# $myc1->getDoomsday();
#
# print "- calling getDoomsday()\n";
# $myc2->getDoomsday();
#
# print "- calling SharedHashFile::createSharedHashFile()\n";
# SharedHashFile::createSharedHashFile();

# $start = microtime(true);
# $count = 0;
# while ($count < 5000000)
# {
# 	//dummy1();
# 	$count ++;
# }
# $end = microtime(true);
# $elapsed = $end - $start;
# printf("- calling *nothing* very many times took %f seconds or %u calls per second\n", $elapsed, $count / $elapsed);
#
# $start = microtime(true);
# $count = 0;
# while ($count < 500000)
# {
# 	dummy1();
# 	$count ++;
# }
# $end = microtime(true);
# $elapsed = $end - $start;
# printf("- calling dummy1() very many times took %f seconds or %u calls per second\n", $elapsed, $count / $elapsed);
#
# $start = microtime(true);
# $count = 0;
# while ($count < 500000)
# {
# 	dummy2(true);
# 	$count ++;
# }
# $end = microtime(true);
# $elapsed = $end - $start;
# printf("- calling dummy2(true) very many times took %f seconds or %u calls per second\n", $elapsed, $count / $elapsed);
#
# $start = microtime(true);
# $count = 0;
# while ($count < 500000)
# {
# 	dummy3("key", "val");
# 	$count ++;
# }
# $end = microtime(true);
# $elapsed = $end - $start;
# printf("- calling dummy3('key', 'val') very many times took %f seconds or %u calls per second\n", $elapsed, $count / $elapsed);
#
# $start = microtime(true);
# $count = 0;
# while ($count < 500000)
# {
# 	$result = dummy4("key", "val");
# 	$count ++;
# }
# $end = microtime(true);
# $elapsed = $end - $start;
# printf("- calling tocopy=dummy4('key', 'val') very many times took %f seconds or %u calls per second\n", $elapsed, $count / $elapsed);
#
# $start = microtime(true);
# $count = 0;
# while ($count < 500000)
# {
# 	$result = dummy5("key", "val");
# 	$count ++;
# }
# $end = microtime(true);
# $elapsed = $end - $start;
# printf("- calling copied=dummy5('key', 'val') very many times took %f seconds or %u calls per second\n", $elapsed, $count / $elapsed);

?>

