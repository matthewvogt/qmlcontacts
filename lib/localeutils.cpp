/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include <QDebug>
#include <string.h>
#include <unicode/unistr.h>
#include <unicode/locid.h>
#include <unicode/coll.h>
#include <unicode/uchar.h>
#include <unicode/ulocdata.h>
#include <unicode/ustring.h>
#include <unicode/uchriter.h>

#include "localeutils.h"

LocaleUtils *LocaleUtils::mSelf = 0;

LocaleUtils::LocaleUtils(QObject *parent) :
    QObject(parent)
{
    int collType = LocaleUtils::Default;
    if (getCountry() == QLocale::Germany)
        collType = LocaleUtils::PhoneBook;

    initCollator(collType);
}

LocaleUtils::~LocaleUtils()
{
}

LocaleUtils *LocaleUtils::self()
{
    if (!mSelf) {
        mSelf = new LocaleUtils();
    }

    return mSelf;
}

QString LocaleUtils::getLanguage() const
{
    return QLocale::system().name();
}

QLocale::Country LocaleUtils::getCountry() const
{
    return QLocale::system().country();
}

QStringList LocaleUtils::getAddressFieldOrder() const
{
    QStringList fieldOrder;
    QLocale::Country country = getCountry();

    if ((country == QLocale::China) || (country == QLocale::Taiwan))
        fieldOrder << "country" << "region" << "locale" << "street" << "street2" << "zip";
    else if (country == QLocale::Japan)
        fieldOrder << "country" << "zip" << "region" << "locale" << "street" << "street2";
    else if ((country == QLocale::DemocraticRepublicOfKorea) ||
             (country == QLocale::RepublicOfKorea))
        fieldOrder << "country" << "region" << "locale" << "street" << "street2" << "zip";
    else
        fieldOrder << "street" << "street2" << "locale" << "region" << "zip" << "country";

    return fieldOrder;
}

bool LocaleUtils::needPronounciationFields() const {
    QStringList fieldOrder;
    QLocale::Country country = getCountry();

    if (country == QLocale::Japan)
        return true;
    return false;
}

bool LocaleUtils::checkForAlphaChar(QString str)
{
    const ushort *strShort = str.utf16();
    UnicodeString uniStr = UnicodeString(static_cast<const UChar *>(strShort));

    //REVISIT: Might need to use a locale aware version of char32At()
    return u_hasBinaryProperty(uniStr.char32At(0), UCHAR_ALPHABETIC);
}

bool LocaleUtils::initCollator(int collType, QString locale)
{
    //Get the locale in a ICU supported format
    if (locale == "")
        locale = getLanguage();
   
    switch (collType) {
        case PhoneBook:
            locale += "@collation=phonebook";
            break;
        case Pinyin:
            locale += "@collation=pinyin";
            break;
        case Traditional:
            locale += "@collation=traditional";
            break;
        case Stroke:
            locale += "@collation=stroke";
            break;
        case Direct:
            locale += "@collation=direct";
            break;
        default:
            locale += "@collation=default";
    }

    const char *name = locale.toLatin1().constData();
    Locale localeName = Locale(name);

    UErrorCode status = U_ZERO_ERROR;
    mColl = (RuleBasedCollator *)Collator::createInstance(localeName, status);
    if (!U_SUCCESS(status))
        return false;

    QLocale::Country country = getCountry();
    if ((country == QLocale::DemocraticRepublicOfKorea) ||
        (country == QLocale::RepublicOfKorea) || (country == QLocale::Japan)) {

        //ASCII characters should be sorted after
        //non-ASCII characters for some languages
        UnicodeString rules = mColl->getRules();
        rules += "< a,A< b,B< c,C< d,D< e,E< f,F< g,G< h,H< i,I< j,J < k,K"
                 "< l,L< m,M< n,N< o,O< p,P< q,Q< r,R< s,S< t,T < u,U< v,V"
                 "< w,W< x,X< y,Y< z,Z";
        mColl = new RuleBasedCollator(rules, status);
        if (!U_SUCCESS(status))
            mColl = (RuleBasedCollator *)Collator::createInstance(localeName, status);
    }

    if (U_SUCCESS(status))
        return true;
    return false;
}

int LocaleUtils::compare(QString lStr, QString rStr)
{
    if (lStr == "#") {
        return false;
    }
    if (rStr == "#") {
        return true;
    }

    //Convert strings to UnicodeStrings
    const ushort *lShort = lStr.utf16();
    UnicodeString lUniStr = UnicodeString(static_cast<const UChar *>(lShort));
    const ushort *rShort = rStr.utf16();
    UnicodeString rUniStr = UnicodeString(static_cast<const UChar *>(rShort));

    if (!mColl) {
        //No collator set, use fall back
        return QString::localeAwareCompare(lStr, rStr) < 0;
    }

    Collator::EComparisonResult res = mColl->compare(lUniStr, rUniStr);
    if (res == Collator::LESS)
        return -1;

    if (res == Collator::GREATER)
        return 1;

    return 0;
}

bool LocaleUtils::isLessThan(QString lStr, QString rStr)
{
    int ret;

    ret = compare(lStr, rStr);
    if (ret == -1) 
        return true;

    return false;
}

QString LocaleUtils::getExemplarForString(QString str)
{
    QStringList indexes = getIndexBarChars();
    int i = 0;

    for (; i < indexes.size(); i++) {
        if (isLessThan(str, indexes.at(i))) {
            if (i == 0) {
                return str;
            }
            return indexes.at(i-1);
        }
    }
    
    return QString(tr("#"));
}

QString LocaleUtils::getBinForString(QString str)
{
    //REVISIT: Might need to use a locale aware version of toUpper() and at()
    if (!checkForAlphaChar(str))
        return QString(tr("#"));

    QString temp(str.at(0).toUpper());
    
    //The proper bin for these locales does not correspond
    //with a bin listed in the index bar
    QLocale::Country country = getCountry();
    if ((country == QLocale::Taiwan) || (country == QLocale::China))
        return temp;

    return getExemplarForString(temp);
}

QStringList LocaleUtils::getIndexBarChars()
{
    UErrorCode  status = U_ZERO_ERROR;
    QStringList default_list = QStringList() << "A" << "B" << "C" << "D" << "E"
                                             << "F" << "G" << "H" << "I" << "J"
                                             << "K" << "L" << "M" << "N" << "O"
                                             << "P" << "Q" << "R" << "S" << "T"
                                             << "U" << "V" << "W" << "Y" << "Z";
    QStringList list;

    QLocale::Country country = getCountry();
    QString locale = getLanguage();
    const char *name = locale.toLatin1().constData();

    //REVISIT: ulocdata_getExemplarSet() does not return the index characters
    //We need to query the locale data directly using the resource bundle 
    UResourceBundle *resource = ures_open(NULL, name, &status);

    if (!U_SUCCESS(status))
        return default_list;

    qint32 size;
    const UChar *indexes = ures_getStringByKey(resource,
                                               "ExemplarCharactersIndex",
                                               &size, &status);
    if (!U_SUCCESS(status))
        return default_list;

    //REVISIT:  This is work around for an encoding issue with KOR chars
    //returned by ICU. Use the compatiblity Jamo unicode values instead
    if ((country == QLocale::DemocraticRepublicOfKorea) ||
       (country == QLocale::RepublicOfKorea)) {
        int i = 0;
        static const QChar unicode[] = {0x3131, 0x3134, 0x3137, 0x3139, 0x3141,
                                        0x3142, 0x3145, 0x3147, 0x3148, 0x314A,
                                        0x314B, 0x314C, 0x314D, 0x314E};
        size = sizeof(unicode) / sizeof(QChar);
        QString jamo = QString::fromRawData(unicode, size);

        for (i = 0; i < jamo.length(); i++)
            list << jamo.at(i);
    }

    else {
        UCharCharacterIterator iter = UCharCharacterIterator(indexes, size);
        UChar c = iter.first();

        for (; c != CharacterIterator::DONE; c = iter.next()) {
            QString temp(c);
            if ((c != ' ') && (c != '[') && (c != ']')) { 
                //Check for exemplars that are two characters
                //These are denoted by '{}'
                if (c == '{') {
                    c = iter.next();
                    temp = "";
                    for (; c != '}'; c = iter.next())
                        temp += QString(c);
                }
                list << temp;
            }
        }
    }

    ures_close(resource);
    if (list.isEmpty())
        return default_list;

    if ((country == QLocale::Taiwan) || (country == QLocale::Japan) ||
       (country == QLocale::DemocraticRepublicOfKorea) ||
       (country == QLocale::RepublicOfKorea))
        list << "A" << "Z";

    list << QString(tr("#"));
    return list;
}

