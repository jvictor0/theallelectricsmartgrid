#!/usr/bin/env python3
"""Run a read-only SQL query against the ladder survey, returning CSV."""

import argparse
import csv
import sys

import duckdb


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('database')
    parser.add_argument('sql', help='SQL text, or @path/to/query.sql')
    args = parser.parse_args()
    sql = args.sql
    if sql.startswith('@'):
        with open(sql[1:]) as stream:
            sql = stream.read()
    with duckdb.connect(args.database,read_only=True) as connection:
        connection.execute("SET memory_limit='96GB'")
        connection.execute('SET threads=16')
        result=connection.execute(sql)
        writer=csv.writer(sys.stdout)
        writer.writerow(column[0] for column in result.description)
        while batch:=result.fetchmany(4096):
            writer.writerows(batch)


if __name__ == '__main__':
    main()
