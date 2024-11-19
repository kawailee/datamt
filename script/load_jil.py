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
from typing import IO
import sqlite3
import traceback

# Install the default signal handler.
from signal import signal, SIGPIPE, SIG_DFL
signal(SIGPIPE, SIG_DFL)

usage=\
'''
	load_jil.py

	load_jil.py -f jil.doc

	To load jil.doc to ~/db/AutosysJob.db

	-	parse jil.doc and flatten out job/box per line
	-	drop and create table KLAutosysJobs
	-	.import using sqlite3

'''

class Job:
	def __init__(self):
		self.job_name = ''
		self.machine_name = ''
		self.script_name = ''
		self.command = ''
		self.job_type = ''
		self.box_name = ''
		self.date_conditions = ''
		self.days_of_week = ''
		self.start_times = ''
		self.timezone = ''
		self.start_mins = ''
		self.run_window = ''
		self.condition = ''
		self.box_success = ''
		return

	def reset(self, job_name:str, job_type:str):
		self.job_name = job_name
		self.machine_name = ''
		self.script_name = ''
		self.command = ''
		self.job_type = job_type
		self.box_name = ''
		self.date_conditions = ''
		self.days_of_week = ''
		self.start_times = ''
		self.timezone = ''
		self.start_mins = ''
		self.run_window = ''
		self.condition = ''
		self.box_success = ''
		return

	@staticmethod
	def remove_quotes(s:str) -> str:
		if len(s) < 2:
			return s
		if s[0] == '"' and s[0] == '"':
			return Job.remove_quotes(s[1:-1])
		if s[0] == '\'' and s[0] == '\'':
			return Job.remove_quotes(s[1:-1])
		else:
			return s
	
	@staticmethod
	def parse(j:'Job', file: IO[str], line:str)->int:
		readyFlag = 0
		line_arr = line.split()
		if len(line_arr) <= 0:
			return readyFlag

		if line_arr[0] == 'insert_job:':
			if j.job_name != '':
				Job.flush_to_file(j, file)
				j.reset(line_arr[1], line_arr[3])
			j.job_name = line_arr[1]
			j.job_type = line_arr[3]

		elif line_arr[0] == 'box_name:':
			j.box_name = line_arr[1]

		elif line_arr[0] == 'command:':
			# need to extract script_name from command
			command_string = line.replace('command:','').strip()
			j.command = Job.remove_quotes(command_string)

			# need to extract script_name from command
			command_array = j.command.split()
			script_name = command_array[0]
			if script_name == 'echo':
				script_name = j.command.replace('echo', '').strip()
				script_name = Job.remove_quotes(script_name)
			else:
				script_name_array = script_name.split('/')
				script_name = script_name_array[-1]

			if script_name == 'run_JobsByBusD':
				subcommand = command_array[4]
				script_name = subcommand.split('/')[-1]

			if script_name == 'run_LastDayJobs':
				if command_array[2] == '-c':
					subcommand = command_array[4]
					script_name = subcommand.split('/')[-1]
				else:
					subcommand = command_array[2]
					script_name = subcommand.split('/')[-1]

			j.script_name = script_name


		elif line_arr[0] == 'machine:':
			j.machine_name = line_arr[1]

		elif line_arr[0] == 'owner:':
			pass

		elif line_arr[0] == 'permission:':
			pass

		elif line_arr[0] == 'date_conditions:':
			j.date_conditions = line_arr[1]

		elif line_arr[0] == 'days_of_week:':
			j.days_of_week = line_arr[1]

		elif line_arr[0] == 'start_times:':
			#remove quote
			j.start_times = Job.remove_quotes(line_arr[1])

		elif line_arr[0] == 'start_mins:':
			j.start_mins = line_arr[1]

		elif line_arr[0] == 'timezone:':
			j.timezone = line_arr[1]

		elif line_arr[0] == 'run_window:':
			#remove double quote
			j.run_window = line_arr[1][1:-1]

		elif line_arr[0] == 'condition:':
			j.condition = ' '.join(line_arr[1:])

		elif line_arr[0] == 'box_success:':
			j.box_success = ' '.join(line_arr[1:])

		else:
			pass
		#print(line)
		return readyFlag

	# 
	@staticmethod
	def flush_to_file(j:'Job', file: IO[str]):
		field_sep = '\t'
		if file:
			file.write(j.job_name)
			file.write(field_sep)
			file.write(j.machine_name)
			file.write(field_sep)
			file.write(j.script_name)
			file.write(field_sep)
			file.write(j.command)
			file.write(field_sep)
			file.write(j.job_type)
			file.write(field_sep)
			file.write(j.box_name)
			file.write(field_sep)
			file.write(j.date_conditions)
			file.write(field_sep)
			file.write(j.days_of_week)
			file.write(field_sep)
			file.write(j.start_times)
			file.write(field_sep)
			file.write(j.timezone)
			file.write(field_sep)
			file.write(j.start_mins)
			file.write(field_sep)
			file.write(j.run_window)
			file.write(field_sep)
			file.write(j.condition)
			file.write(field_sep)
			file.write(j.box_success)
			file.write('\n')
		return

def get_job_all_detail(jil_file:str, jil_flat_file:str):
	print(f'Extract details of {jil_file} to {jil_flat_file}')
	j = Job()
	with open(jil_flat_file,'w') as fo:
		with open(jil_file,'r') as fi:
			for line in fi:
				#print(line.strip())
				Job.parse(j, fo, line.strip())
				#fo.write(line.strip())
				#fo.write('\n')
		if j.job_name != '':
			Job.flush_to_file(j, fo)

	return

def drop_create_load_KLAUtosysJobs(db_file:str, jil_flat_file:str):

	print(f'load {jil_flat_file} to {db_file}')

	sql=f'''
drop table if exists KLAutosysJobs;
create table if not exists KLAutosysJobs
(
	job_name		TEXT
,	machine_name	TEXT
,	script_name		TEXT
,	command			TEXT
,	job_type		TEXT
,	box_name		TEXT
,	date_conditions	TEXT
,	date_of_week	TEXT
,	start_times		TEXT
,	timezone		TEXT
,	start_mins		TEXT
,	run_window		TEXT
,	condition		TEXT
,	box_success		TEXT
);

create unique index if not exists KLAutosysJobs_PrimaryIdx on KLAutosysJobs(job_name);

.separator "\t"
.import "{jil_flat_file}" KLAutosysJobs
select 'Number of Jobs' as 'Tag', count(*) 'RowCount' from KLAutosysJobs;
	'''

	p = subprocess.Popen(f'sqlite3 {db_file}', stdout=subprocess.PIPE, stdin=subprocess.PIPE, stderr=sys.stderr,
		text=True, encoding='utf8', errors='replace', shell=True)
	
	p.stdin.write(sql)
	p.stdin.flush()
	p.stdin.close()

	for line in p.stdout:
		print(line.rstrip())
	
	rc = p.wait()

	return

def get_list_of_success_jobs(condition:str) -> list[str]:
	jobs = []

	matches = re.findall(r's\([a-zA-Z0-9_]*\)', condition)
	jobs = [ x.strip()[2:-1] for x in matches]
	return jobs

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

def build_job_dependency(db_file:str) -> list[str,str]:

	print(f'Scan and build job dependency in database {db_file}')

	con = None
	cur = None

	dep = []

	try:
		table_name = 'KLAutosysJobs'
		con = sqlite3.connect(db_file)
		cur = con.cursor()

		cur.execute(f'select job_name, job_type, condition from {table_name} where condition != "";')
		rows = cur.fetchall()
		con.close

		for row in rows:
			#print( row[2] )
			jobs_within_box = expand_box_to_jobs(con, row[0])

			jobs = get_list_of_success_jobs(row[2])
			djobs_within_box = []
			djobs_within_box = expand_list_to_jobs(con, jobs)

			#	DEBUG
			'''
			if '752_T_Genesis_RegCGMT_Bal_PP' in jobs_within_box:
				print('===>>', row[0])
				print('===>>', jobs_within_box)
				print('===>>', jobs)
				print('===>>', djobs_within_box)
			'''

			for j in jobs_within_box:
				for k in djobs_within_box:
					dep.append( (j, k) )
		
		con = None

	except Exception as e:
		print(e)
		traceback.print_exc()
	finally:
		if con:
			con.close()
			con = None

	return dep

def save_deplist(db_file:str, deplist:list[str,str]):

	print(f'Save job dependency in database {db_file}')

	con = None
	cur = None

	try:
		con = sqlite3.connect(db_file)
		cur = con.cursor()

		cur.execute('DROP TABLE IF EXISTS JobDep')
		cur.execute('CREATE TABLE IF NOT EXISTS JobDep( job_name TEXT, dep_job_name TEXT)')
		cur.execute('CREATE UNIQUE INDEX JobDep_Idx0 on JobDep( job_name, dep_job_name)')

		for dep in deplist:
			cur.execute('INSERT OR REPLACE INTO JobDep (job_name, dep_job_name) values (?,?)', dep)

		con.commit()
		con.close()		
		con = None

	except Exception as e:
		print(e)
		traceback.print_exc()
	finally:
		if con:
			con.close()
			con = None

	return


def main(argv):
	# https://docs.python.org/3/library/argparse.html
	parser = argparse.ArgumentParser()
	parser.add_argument('-t', '--test', action='store_true', help='just test; no rename')
	parser.add_argument('-i', '--init', action='store', type=int, help='initial prefix number. e.g. 21')
	parser.add_argument('-f', '--file', action='store', default='jil.doc', help='specify input file name, e.g. jil.doc')
	parser.add_argument('-d', '--db', action='store', default='~/db/AutosysJob.db', help='specify db file, e.g. ~/db/AutosysJob.db')
	args = parser.parse_args()

	jil_file = os.path.expanduser(args.file)
	db_file = os.path.expanduser(args.db)
	jil_file_base_name = os.path.basename(jil_file)

	if os.path.exists(db_file):
		pass
	else:
		print(usage)
		sys.exit(-1)

	if os.path.exists(jil_file):
		pass
	else:
		print(usage)
		sys.exit(-1)

	jil_flat_file = f'/tmp/{jil_file_base_name}.tmp'
	get_job_all_detail(jil_file, jil_flat_file)

	drop_create_load_KLAUtosysJobs(db_file, jil_flat_file)

	deplist = build_job_dependency(db_file)

	save_deplist(db_file, deplist)

if __name__ == '__main__':
	main(sys.argv)

