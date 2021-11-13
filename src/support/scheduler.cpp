#
/*
 *    Copyright (C) 2021
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Computing
 *
 *    This file is part of Qt-DAB
 *    Qt-DAB is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    Qt-DAB is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Qt-DAB; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include	"scheduler.h"
#include	"radio.h"
#include	"dab-constants.h"
#include	<stdio.h>
#include	<iostream>
#include	<fstream>
#include	<string>
#include	<QStringList>

#define	SWITCHTIME	15
#define	MINUTES_PER_DAY	(24 * 60)
#define	MINUTES_PER_HOUR 60
#define	WAITING_PERIOD	600000
//
///
//	Note
//	In this version, we maintain time as being relative to 1 jan
//	wakeupTime is given in minutes,
//	currentTime is eventually computed in seconds
//	the time difference then is in seconds and the delay in msec
	Scheduler::Scheduler (RadioInterface *mr, const QString &fileName):
	                              myWidget (nullptr) {
	myWidget. resize (240, 200);
	myWidget. setWidgetResizable(true);
	this	-> fileName	= fileName;

	referenceDate	= QDate::currentDate ();
	tableWidget 	= new QTableWidget (0, 3);
	tableWidget	-> setColumnWidth (0, 120);
	myWidget. setWidget(tableWidget);
	tableWidget 	-> setHorizontalHeaderLabels (
	            QStringList () << tr ("service name     ") << tr ("day") << tr ("time"));
	connect (tableWidget, SIGNAL (cellDoubleClicked (int, int)),
	         this, SLOT (removeRow (int, int)));
	wakeupTimer. setSingleShot (true);
	wakeupTimer. setInterval   (1000000);
	connect (&wakeupTimer, SIGNAL (timeout ()),
	         this, SLOT (handle_timeOut ()));
	connect (this, SIGNAL (timeOut (const QString &)),
	         mr, SLOT (scheduler_timeOut (const QString &)));
	this	-> wakeupTime = 365 * MINUTES_PER_DAY;
	read (fileName);
}

	Scheduler::~Scheduler () {
int16_t	i;
int16_t	rows	= tableWidget -> rowCount ();
	myWidget. hide ();
	wakeupTimer. stop ();
	dump (fileName);
	for (i = rows; i > 0; i --)
	   tableWidget -> removeRow (i);
	delete	tableWidget;
}

void	Scheduler::show	() {
	myWidget. show ();
}

void	Scheduler::hide	() {
	myWidget. hide ();
}

void	Scheduler::clear	() {
int16_t	rows	= tableWidget -> rowCount ();
	myWidget. hide ();
	wakeupTimer. stop ();
	for (int i = rows; i > 0; i --)
	   tableWidget -> removeRow (i);
}

const char *weekdays [] = {
	"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
};

int	dayDiff (const QString &dayRef, const QString &dayTest) {
int index_1, index_2;

	for (index_1 = 0; weekdays [index_1] != dayRef; index_1 ++);
	for (index_2 = 0; weekdays [index_2] != dayTest; index_2 ++);
	if (index_1 > index_2)
	   index_2 += 7;
	return index_2 - index_1;
}

void	Scheduler::addRow (const QString &name, int day,
	                              int hours, int minutes) {
int16_t	row		= tableWidget -> rowCount ();
QString	recordTime	= QString::number (hours / 10) +
	                           QString::number (hours % 10);
QDate	wakeupDate	= QDate::currentDate ();	// default
QDate	currentDate	= QDate::currentDate ();	// 
QTime	currentTime	= QTime::currentTime ();


	if (tableWidget -> rowCount () == 0) 
	   referenceDate = QDate::currentDate ();
	wakeupTimer. stop ();
	wakeupDate	= wakeupDate. addDays (day);
	int theNow	= - currentDate. daysTo (referenceDate);
	int delayDays	= - wakeupDate. daysTo (referenceDate);

	int currentSeconds	= (theNow * MINUTES_PER_DAY +
	                           currentTime. hour () * MINUTES_PER_HOUR + 
	                           currentTime. minute ()) * 60 +
	                                     currentTime. second ();
	int wakeupTime	= (delayDays * MINUTES_PER_DAY +
	                   hours * MINUTES_PER_HOUR + minutes) * 60;
	QString	startDay	= QDate::shortDayName (referenceDate. dayOfWeek ());
	QString wakeupDay	= QDate::shortDayName (wakeupDate. dayOfWeek ());
	recordTime. append (":");
	recordTime. append (QString::number (minutes / 10) +
	                             QString::number (minutes % 10));

	tableWidget	-> insertRow (row);
	QTableWidgetItem *item0	= new QTableWidgetItem;
	item0		-> setTextAlignment (Qt::AlignRight |Qt::AlignVCenter);
	tableWidget	-> setItem (row, 0, item0);

	QTableWidgetItem *item1 = new QTableWidgetItem;
	item1		-> setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
	tableWidget	-> setItem (row, 1, item1);

	QTableWidgetItem *item2 = new QTableWidgetItem;
	item2		-> setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
	tableWidget	-> setItem (row, 2, item2);

	tableWidget	-> setCurrentItem (item0);
	tableWidget	-> item (row, 0) -> setText (name);
	tableWidget	-> item (row, 1) -> setText (wakeupDay);
	tableWidget	-> item (row, 2) -> setText (recordTime);
	
	if (wakeupTime < this -> wakeupTime) {
	   this -> wakeupTime = wakeupTime;
	   this	-> wakeupIndex = tableWidget -> rowCount () - 1;
	};

	int tempDelay	= this -> wakeupTime - currentSeconds;
//	fprintf (stderr, "waiting time %d hours %d minutes\n",
//	                       tempDelay / MINUTES_PER_HOUR / 60,
//	                       tempDelay / MINUTES_PER_HOUR % 60);
	int theDelay	= this -> wakeupTime  - currentSeconds - SWITCHTIME;
	if (theDelay <= 1000) 
	   theDelay = 1000;	// milliseconds
	else
	   theDelay = 1000 * theDelay;
	if (theDelay > WAITING_PERIOD)
	   theDelay = WAITING_PERIOD;
	wakeupTimer. setInterval (theDelay);
	wakeupTimer. start (theDelay);
	show ();
}
//
void	Scheduler::removeRow (int row, int column) {
	(void) column;
	wakeupTimer. stop ();
	tableWidget	-> removeRow (row);
	if (tableWidget -> rowCount () == 0) {
	   wakeupTime	= 365 * MINUTES_PER_DAY;
	   wakeupTimer. stop ();
	   hide ();
	   return;
	}
//
//	and recompute the wakeupTime
	QString startDay	= QDate::shortDayName (referenceDate. dayOfWeek ());
	for (int i = 0; i < tableWidget -> rowCount (); i ++) {
	   QString testTime	= tableWidget -> item (i, 2) -> text ();
	   QStringList test	= testTime. split (":");
	   int hours		= test. at (0). toInt ();
	   int minutes		= test. at (1). toInt ();

	   QString wakeupDay	= tableWidget -> item (i, 1) -> text ();
	   int delayDays	= dayDiff (startDay, wakeupDay);
	   int testSeconds	= (delayDays * MINUTES_PER_DAY +
	                           hours * MINUTES_PER_HOUR + minutes) * 60;
	   if (testSeconds < this -> wakeupTime) {
	      this -> wakeupTime = testSeconds;
	      wakeupIndex = i;
	   }
	}

	QDate currentDate	= QDate::currentDate ();
	int theNow		= - currentDate. daysTo (referenceDate);
	QTime	currentTime	= QTime::currentTime ();
	int currentSeconds	= (theNow * MINUTES_PER_DAY +
	                          currentTime. hour () * MINUTES_PER_HOUR +
	                          currentTime. minute ()) * 60 +
	                                    currentTime. second ();

	int64_t theDelay
	             = this ->  wakeupTime - currentSeconds - SWITCHTIME; // seconds
	if (theDelay <= 0) 
	   theDelay = 1000;	// milliseconds
	else
	   theDelay = 1000 * theDelay;

	if (theDelay > WAITING_PERIOD)
	   theDelay = WAITING_PERIOD;
	wakeupTimer. setInterval (theDelay);
	wakeupTimer. start (theDelay);
}
//
//	Each 10 minutes we check the time, to avoid too large
//	delays
void	Scheduler::handle_timeOut () {
QString	service	= tableWidget -> item (wakeupIndex, 0) -> text ();

QDate currentDate       = QDate::currentDate ();
int theNow              = - currentDate. daysTo (referenceDate);
QTime currentTime	= QTime::currentTime ();
int	currentSeconds	= (theNow * MINUTES_PER_DAY +
                           currentTime. hour () * MINUTES_PER_HOUR +
                           currentTime. minute ()) * 60 +
                                            currentTime. second ();

	if (currentSeconds >= wakeupTime - SWITCHTIME) {
	   wakeupTime	= 365 * MINUTES_PER_DAY;	// minutes
	   removeRow (wakeupIndex, 0);

	
//	and recompute the nearest wakeupTime
	   QString startDay	=
	             QDate::shortDayName (referenceDate. dayOfWeek ());
	   for (int i = 0; i < tableWidget -> rowCount (); i ++) {
	      QString testTime = tableWidget -> item (i, 2) -> text ();
	      QStringList test = testTime. split (":");
	      int hours	= test. at (0). toInt ();
	      int minutes	= test. at (1) . toInt ();
	      QString wakeupDay	= tableWidget -> item (i, 1) -> text ();
	      int delayDays	= dayDiff (startDay, wakeupDay);
	      int testSeconds	= (delayDays * MINUTES_PER_DAY +
	                           hours * MINUTES_PER_HOUR + minutes) * 60;

	      if (testSeconds < this -> wakeupTime) {
	         this -> wakeupTime = testSeconds;
	         wakeupIndex = i;
	      }
	   }
	   service = service. trimmed ();
	   fprintf (stderr, "we starten de service %s\n",
	                                 service. toLatin1 (). data ());
	   emit timeOut (service);
	   return;
	}

	int theDelay	=
	           this -> wakeupTime - currentSeconds - SWITCHTIME; // seconds
	if (theDelay <= 500) 
	   theDelay = 1000;	// milliseconds
	else
	   theDelay = 1000 * theDelay;	// in millseconds

	if (theDelay > WAITING_PERIOD)
	   theDelay = WAITING_PERIOD;
	wakeupTimer. setInterval (theDelay);
	wakeupTimer. start (theDelay);
}

void	Scheduler::dump (const QString &file) {
FILE *dumpFile	= fopen (file. toLatin1 (). data (), "w");

	if (dumpFile == nullptr)
	   return;

	QString startDay     =
                     QDate::shortDayName (referenceDate. dayOfWeek ());

	fprintf (dumpFile, "// dump schedule file with %d elements\n",
	                    tableWidget -> rowCount ());
	fprintf (dumpFile, "reference: %d %d %d\n",
	                     referenceDate. year (),
	                     referenceDate. month (),
	                     referenceDate. day ());
	for (int row = 0; row < tableWidget -> rowCount (); row ++) {
	   QString service	=
	            tableWidget -> item (row, 0) -> text ();
	   QString wakeupDay	=
	            tableWidget -> item (row, 1) -> text ();
	   int delayDays	= dayDiff (startDay, wakeupDay);
	   QString wakeupTime	=
	            tableWidget -> item (row, 2) -> text ();
	   QStringList t = wakeupTime . split (":");
           if (t. size () != 2)
              continue;
           int hours    = t [0]. toInt ();
           int minutes  = t [1]. toInt ();

	   fprintf (dumpFile, "element: | %s | %d %d %d %d\n",
	                    service. toLatin1 (). data (),
	                    delayDays, hours, minutes, -(delayDays + hours + minutes));
	}

	fclose (dumpFile);
}
//
//	when reading in an "old" schedule, we have to translate the dates
//	by subtracting the nr of days passed since the "old" schedule
//	was made. Of course, schedule elements in the past
//	are ignored
void	Scheduler::read (const QString &fileName) {
std::ifstream f (fileName. toLatin1 (). data ());
QDate currentDate = QDate::currentDate ();
QTime currentTime = QTime::currentTime ();
QDate startDate;

	std::string str;
	size_t amount	= 256;
	while (true) {
	   if (!std::getline (f, str))
	      break;

	fprintf (stderr, "read a line %s\n", str.c_str ());
//      just to be on the safe side
           if (str. size () < 10)
              continue;

	   if (str. find ("//") < str. length ())
	      continue;

	   if (str. find ("reference:") < str. length ()) {
	      int day, month, year;
	      char x [255];
	      sscanf (str. c_str (), "%s %d %d %d",
	                            x, &year, &month, &day);
	      startDate = QDate (year, month, day);
	      fprintf (stderr, "statDate daysTo %d\n",
	                           currentDate. daysTo (startDate));
	      if (currentDate. daysTo (startDate) <= -7) {
	         fprintf (stderr, "too long ago, ignored\n");
	         return;
	      }
	      continue;
	   }

	   if (str. find ("element:") < str.length ()) {
	      std::string service;
	      QString temp	= QString::fromStdString (str);
	      QStringList ss = temp. split ('|');
	      fprintf (stderr, "split count %d\n", ss. count ());
	      for (int i = 0; i < ss. count (); i ++)
	         fprintf (stderr, "%s \n", ss. at (i). toLatin1 (). data ());
	      if (ss. count () != 3)
	         return;
	      service = ss. at (1). toLatin1 (). data ();
	      std::string rest = ss. at (2). toLatin1 (). data ();
	      int delayDays, hour, minutes, sum;
	      sscanf (rest. c_str (), "%d %d %d %d",
	                    &delayDays, &hour, &minutes, &sum);
	      fprintf (stderr, "na de scan: delayDays %d\n", delayDays);
	      if (sum + delayDays + hour + minutes != 0)
	         continue;
//	first a simple validity check on the data
	      if ((delayDays < 0) || (delayDays >= 7))
	         continue;
	      if ((hour < 0) || (hour > 23))
	         continue;
	      if ((minutes < 0) || (minutes > 59))
	         continue; 
//	Then, a check to see if it is still a meaningful entry,
//	i.e. delayDay is not in the past relative to the current time
	      delayDays	+= currentDate. daysTo (startDate);
//
//	Note that, if delayDays > 0, test will always yield false
	      int testMinutes	= (delayDays * MINUTES_PER_DAY +
	                           hour * MINUTES_PER_HOUR + minutes);
	      int refMinutes	= currentTime. hour () * MINUTES_PER_HOUR +
	                          currentTime. minute ();
	      fprintf (stderr, "delayDays = %d\n", delayDays);
	      if (testMinutes < refMinutes) 
	         fprintf (stderr, "schedule for service %s is out of date\n",
	                                      service);
	      else
	         addRow (service. c_str (), delayDays, hour, minutes);
	   }
	}
}
