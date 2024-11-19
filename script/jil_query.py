#!/usr/bin/python3 -B
# -*- coding:utf-8 -*-

import os
import sys
import glob
import re
from datetime import date
from datetime import datetime
import argparse
import functools
import shutil
import subprocess
import sqlite3
from typing import IO

# Install the default signal handler.
from signal import signal, SIGPIPE, SIG_DFL
signal(SIGPIPE, SIG_DFL)

usage=\
'''
	jil_query.py

	jil_query.py -d ~/db/AutosysJob.db

	-	a

'''

def expand_box_to_jobs(con:sqlite3.Connection,box_name:str) -> list[str]:
	jobs = []
	
	cur = con.cursor()
	cur.execute(f'select job_name, job_type from KLAutosysJobs where box_name = :box_name', (box_name,))
	rows = cur.fetchall()
	cur.close()

	if len(rows) == 0:
		jobs.append(box_name)
	else:
		for row in rows:
			if row[1] == 'BOX':
				jobs = jobs + expand_box_to_jobs(con, row[0])
			else:
				jobs.append(row[0])

	#	convert list ot unique_list:	unique_list = list(set(my_list))
	try:
		jobs2 = list(set(jobs))
	except Exception as e:
		print(e)
		print (jobs)
		raise(e)

	return jobs

def expand_list_to_jobs(con:sqlite3.Connection,boxjobs:list[str]) -> list[str]:
	jobs = []
	#	convert list ot unique_list:	unique_list = list(set(my_list))

	for job in boxjobs:
		jobs = jobs + expand_box_to_jobs(con, job)

	#	convert list ot unique_list:	unique_list = list(set(my_list))
	try:
		jobs2 = list(set(jobs))
	except Exception as e:
		print(e)
		print (jobs)
		raise(e)
	
	return jobs

def build_job_dep_dict(con:sqlite3.Connection) -> dict:
	dict = {}
	cur = None

	try:
		cur = con.cursor()
		cur.execute('select job_name, dep_job_name from JobDep order by job_name, dep_job_name')
		rows = cur.fetchall()
		cur.close()
		cur = None

		for row in rows:
			if row[0] in dict.keys():
				dep_jobs = dict[row[0]]
				dep_jobs.append(row[1])
				dict.update({row[0]:dep_jobs})
			else:
				dict.update({row[0]:[row[1]]})

	except Exception as e:
		print(e)
		raise(e)
	finally:
		if cur:
			cur.close()
			cur = None
	return dict

def qry_j_dependency(db_file:str, args:argparse):
	print(f'Query job {args.job}:')

	con = None
	cur = None

	boxjobs = []
	try:
		con = sqlite3.connect(db_file)

		cur = con.cursor()
		cur.execute(f'select job_name, job_type from KLAutosysJobs where job_name = :job_name', (args.job, ))
		rows = cur.fetchall()

		if len(rows) > 0:
			if rows[0][1] == 'BOX':
				boxjobs.append(rows[0][0])
				jobs = expand_list_to_jobs(con, boxjobs)
				print(jobs)
			else:
				boxjobs.append(rows[0][0])
				print(boxjobs)
			
			dict = build_job_dep_dict(con)
			print( dict[rows[0][0]] )
		else:
			print(f'job/box {args.job} not found!')
	
		con.close
		con = None
	except Exception as e:
		raise e
	finally:
		if con:
			con.close()
			con = None
	return

def main(argv):
	# https://docs.python.org/3/library/argparse.html
	parser = argparse.ArgumentParser()
	parser.add_argument('-d', '--db', action='store', default='~/db/AutosysJob.db', help='specify db file, e.g. ~/db/AutosysJob.db')
	parser.add_argument('-j', '--job', action='store', help='specify job name')
	args = parser.parse_args()

	db_file = os.path.expanduser(args.db)

	if os.path.exists(db_file):
		pass
	else:
		print(usage)
		sys.exit(-1)

	qry_j_dependency(db_file, args)

if __name__ == '__main__':
	main(sys.argv)

