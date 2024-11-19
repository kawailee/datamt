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

# Install the default signal handler.
from signal import signal, SIGPIPE, SIG_DFL
signal(SIGPIPE, SIG_DFL)

usage=\
'''
	load_jobstatus.py

	load_jobstatus.py -f jobstatus.doc

	To load jobstatus.doc to ~/db/AutosysJob.db

	-	parse jobstatus.doc
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

class JobStatus:
	def __init__(self):
		self.job_name = ''
		self.last_start = ''
		self.last_end = ''
		self.status = ''
		self.dt_last_start = None
		self.dt_last_end = None

	def reset(self, job_name:str):
		self.job_name = job_name
		self.last_start = ''
		self.last_end = ''
		self.status = ''
		self.dt_last_start = None
		self.dt_last_end = None

	def dump(self):
		print(f'{self.job_name}\t{self.last_start}\t{self.last_end}\t{self.status}')
		return

	def update_jobstatus_entry(self, con:sqlite3.Connection):
		cur = con.cursor()
		rs = cur.execute('select job_name, last_start, last_end, status from JobStatus where job_name = :job_name', (self.job_name, ))
		rows = rs.fetchall()
		if len(rows) <= 0:
			cur.execute('insert into JobStatus (job_name, last_start, last_end, status) values(?,?,?,?)', (self.job_name, self.last_start, self.last_end, self.status))
		else:
			if self.last_start != '' and self.last_end == '':
				cur.execute('update JobStatus set last_start=?, status=? where job_name=?', (self.last_start, self.status, self.job_name))
			elif self.last_start == '' and self.last_end != '':
				cur.execute('update JobStatus set last_end=?, status=? where job_name=?', (self.last_end, self.status, self.job_name))
			elif self.last_start != '' and self.last_end != '':
				cur.execute('update JobStatus set last_start=?, last_end=?, status=? where job_name=?', (self.last_start, self.last_end, self.status, self.job_name))
			elif self.last_start == '' and self.last_end == '':
				cur.execute('update JobStatus set status=? where job_name=?', (self.status, self.job_name))

		return

	def parse(self, file: IO[str], line:str)->int:
		rc = 0

		if len(line) <= 0:
			return 0
		
		if line.startswith('Job Name'):
			return 0
		
		if line.startswith('_____'):
			return 0
		
		line_array = line.split()
		self.job_name = line_array[0]
		idx = 1

		if line_array[idx].startswith('-----'):
			self.last_start = ''
			self.dt_last_start = None
			idx = idx + 1
		else:
			self.dt_last_start = datetime.strptime(line_array[idx]+' '+line_array[idx+1],'%m/%d/%Y %H:%M:%S')
			self.last_start = self.dt_last_start.strftime('%Y-%m-%d %H:%M:%S')
			idx = idx + 2

		if line_array[idx].startswith('-----'):
			self.last_end = ''
			self.dt_last_end = None
			idx = idx + 1
		else:
			self.dt_last_end = datetime.strptime(line_array[idx]+' '+line_array[idx+1],'%m/%d/%Y %H:%M:%S')
			self.last_end = self.dt_last_end.strftime('%Y-%m-%d %H:%M:%S')
			idx = idx + 2

		if line_array[idx].endswith('/NE'):
			self.status = 'NE'
		else:
			self.status = line_array[idx][0:2]

		idx = idx + 1

		rc = 1
		return rc

def create_table_jobstatus(con:sqlite3.Connection):
	cur = None
	try:
		cur = con.cursor()
		cur.executescript('''
			BEGIN;
			CREATE TABLE IF NOT EXISTS JobStatus(
					job_name		TEXT
				,	last_start		TEXT
				,	last_end		TEXT
				,	status			TEXT
			);
			CREATE UNIQUE INDEX IF NOT EXISTS JobStatus_Idx0 on JobStatus(job_name);
			COMMIT;
		''')
	finally:
		pass
	return

def update_jobstatus_db(db_file:str, jobstatus_file:str, args:argparse):
	js = None
	con = None

	js = JobStatus()
	
	try:
		con = sqlite3.connect(db_file)
		create_table_jobstatus(con)
		with open(jobstatus_file,'r') as fi:
			for line in fi:
				if JobStatus.parse(js, fi, line.strip()) > 0:
					if args.print:
						js.dump()
					else:
						js.update_jobstatus_entry(con)
		con.commit()
		con.close()
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
	parser.add_argument('-t', '--test', action='store_true', help='just test; no rename')
	parser.add_argument('-i', '--init', action='store', type=int, help='initial prefix number. e.g. 21')
	parser.add_argument('-f', '--file', action='store', default='jobstatus.doc', help='specify input file name, e.g. jobstatus.doc')
	parser.add_argument('-d', '--db', action='store', default='~/db/AutosysJob.db', help='specify db file, e.g. ~/db/AutosysJob.db')
	parser.add_argument('-p', '--print', action='store_true', default=False, help='print out to console')
	args = parser.parse_args()

	jobstatus_file = os.path.expanduser(args.file)
	db_file = os.path.expanduser(args.db)
	jobstatus_file_base_name = os.path.basename(jobstatus_file)

	if os.path.exists(db_file):
		pass
	else:
		print(usage)
		sys.exit(-1)

	if os.path.exists(jobstatus_file):
		pass
	else:
		print(usage)
		sys.exit(-1)

	#jil_flat_file = f'/tmp/{jobstatus_file_base_name}.tmp'
	#get_job_all_detail(jobstatus_file_base_name, jil_flat_file)

	#drop_create_load_KLAUtosysJobs(db_file, jil_flat_file)
	print(f'Loading job staus file {jobstatus_file} to {db_file}')
	update_jobstatus_db(db_file, jobstatus_file, args)

if __name__ == '__main__':
	main(sys.argv)

