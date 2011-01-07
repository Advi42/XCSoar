/**
 ****************************************************************************
 * <P> XML.c - implementation file for basic XML parser written in ANSI C++
 * for portability. It works by using recursion and a node tree for breaking
 * down the elements of an XML document.  </P>
 *
 * @version     V1.08
 *
 * @author      Frank Vanden Berghen
 * based on original implementation by Martyn C Brown
 *
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ****************************************************************************
 */
#ifndef __INCLUDE_XML_NODE__
#define __INCLUDE_XML_NODE__

#include "Compiler.h"

#include <stdlib.h>
#include <tchar.h>

class TextWriter;

// Some common types for char set portable code
#ifdef _UNICODE
    #ifndef LPCTSTR
        #define LPCTSTR const TCHAR *
    #endif /* LPCTSTR */
    #ifndef LPTSTR
        #define LPTSTR TCHAR *
    #endif /* LPTSTR */
    #ifndef TCHAR
        #define TCHAR TCHAR
    #endif /* TCHAR */
#else
    #ifndef LPCTSTR
        #define LPCTSTR const char *
    #endif /* LPCTSTR */
    #ifndef LPTSTR
        #define LPTSTR char *
    #endif /* LPTSTR */
    #ifndef TCHAR
        #define TCHAR char
    #endif /* TCHAR */
#endif

/** Enumeration for XML parse errors. */
typedef enum XMLError
{
  eXMLErrorNone = 0,
  eXMLErrorEmpty,
  eXMLErrorFirstNotStartTag,
  eXMLErrorMissingTagName,
  eXMLErrorMissingEndTagName,
  eXMLErrorNoMatchingQuote,
  eXMLErrorUnmatchedEndTag,
  eXMLErrorUnexpectedToken,
  eXMLErrorInvalidTag,
  eXMLErrorNoElements,
  eXMLErrorFileNotFound
} XMLError;

/** Enumeration used to manage type of data. Use in conjonction with structure XMLNodeContents */
typedef enum XMLElementType
{
  eNodeChild = 0,
  eNodeAttribute = 1,
  eNodeText = 2,
  eNodeNULL = 4
} XMLElementType;

/** Structure used to obtain error details if the parse fails. */
typedef struct XMLResults
{
  enum XMLError error;
  unsigned nLine, nColumn;
} XMLResults;

/** Structure for XML attribute. */
typedef struct
{
  LPCTSTR lpszName;
  LPCTSTR lpszValue;
} XMLAttribute;

struct XMLNodeContents;

typedef struct XMLNode
{
//  friend class XMLNode;
protected:
  typedef struct // to allow shallow copy and "intelligent/smart" pointers (automatic delete):
  {
    /** Element name (=NULL if root) */
    LPCTSTR       lpszName;

    /** Num of child nodes */
    unsigned nChild;

    /** Num of text fields */
    unsigned nText;

    /** Num of attributes */
    unsigned nAttribute;

    /** Whether node is an XML declaration - '<?xml ?>' */
    bool isDeclaration;

    /** Pointer to parent element (=NULL if root) */
    XMLNode *pParent;

    /** Array of child nodes */
    XMLNode *pChild;

    /** Array of text fields */
    LPCTSTR *pText;

    /** Array of attributes */
    XMLAttribute *pAttribute;

    /** order in which the child_nodes,text_fields,clear_fields and */
    unsigned *pOrder;

    unsigned ref_count;
  } XMLNodeData;
  XMLNodeData *d;

  /**
   * Protected constructor: use "parse" functions to get your first
   * instance of XMLNode.
   */
  XMLNode(XMLNode *pParent, LPCTSTR lpszName, bool isDeclaration);

public:
  // You must create your first instance of XMLNode with these 3 parse functions:
  // (see complete explanation of parameters below)

  static XMLNode createRoot(LPCTSTR lpszName);

  static XMLNode parseString   (LPCTSTR     lpszXML, XMLResults *pResults=NULL);
  static XMLNode parseFile     (const char *lpszXML, XMLResults *pResults=NULL);
  static XMLNode openFileHelper(const char *lpszXML);

  // The tag parameter should be the name of the first tag inside the XML file.
  // If the tag parameter is omitted, the 3 functions return a node that represents
  // the head of the xml document including the declaration term (<? ... ?>).

  // If the XML document is corrupted:
  //   - The "openFileHelper" method will stop execution and display an error message.
  //   - The 2 other methods will initialize the "pResults" variable with some information that
  //     can be used to trace the error.
  // you can have a detailed explanation of the parsing error with this function:

  static bool GlobalError;

  /**
   * Parse XML errors into a user friendly string.
   */
  static LPCTSTR getError(XMLError error);

  /**
   * name of the node
   */
  LPCTSTR getName() const;

  /** @return ith text field */
  LPCTSTR getText(unsigned i = 0) const;

  /** nbr of text field */
  unsigned nText() const;

  /** @return ith child node */
  XMLNode getChildNode(unsigned i);

  /**
   * @return ith child node with specific name (return an empty node
   * if failing)
   */
  XMLNode getChildNode(LPCTSTR name, unsigned i);

  /**
   * @return next child node with specific name (return an empty node
   * if failing)
   */
  XMLNode getChildNode(LPCTSTR name, unsigned *i=NULL);

  /** @return the number of child node with specific name */
  unsigned nChildNode(LPCTSTR name) const;

  /** nbr of child node */
  unsigned nChildNode() const;

  /** @return ith attribute */
  XMLAttribute getAttribute(unsigned i);

  /** test if an attribute with a specific name is given */
  bool isAttributeSet(LPCTSTR name) const;

  /**
   * @return ith attribute content with specific name (return a NULL
   * if failing)
   */
  LPCTSTR getAttribute(LPCTSTR name, unsigned i) const;

  /**
   * @return next attribute content with specific name (return a NULL
   * if failing)
   */
  LPCTSTR getAttribute(LPCTSTR name, unsigned *i=NULL) const;

  /**
   * nbr of attribute
   */
  unsigned nAttribute() const;

  /**
   * Create an XML file from the head element.
   *
   * @param writer the stream to write the XML text to

   * @param nFormat 0 if no formatting is required, otherwise nonzero
   * for formatted text with carriage returns and indentation.
   */
  void serialise(TextWriter &writer, int nFormat) const;

  /**
   * Enumerate all the different contents (child, text, clear,
   * attribute) of the current XMLNode. The order is reflecting the
   * order of the original file/string.
   */
  XMLNodeContents enumContents(unsigned i) const;

  /**
   * nbr of different contents for current node
   */
  gcc_pure
  unsigned nElement() const;

  /** is this node empty? */
  gcc_pure
  bool isEmpty() const;

  gcc_pure
  bool isDeclaration() const;

  // to allow shallow copy:
  ~XMLNode();

  /**
   * Shallow copy.
   */
  XMLNode(const XMLNode &A);

  /**
   * Shallow copy.
   */
  XMLNode& operator=(const XMLNode& A);

  static void destroyCurrentBuffer(XMLNodeData *d);

  XMLNode(): d(NULL) {}
  static XMLNode emptyXMLNode;
  static XMLAttribute emptyXMLAttribute;

  // The strings given as parameters for these 4 methods will be free'd by the XMLNode class:

  /**
   * Add a child node to the given element.
   */
  XMLNode AddChild(LPCTSTR lpszName, bool isDeclaration);

  /**
   * Add an attribute to an element.
   */
  XMLAttribute *AddAttribute(LPCTSTR lpszName, LPCTSTR lpszValuev);

  /**
   * Add text to the element.
   */
  LPCTSTR AddText(LPCTSTR lpszValue);

private:
  // these are functions used internally (don't bother about them):
  bool ParseXMLElement(void *pXML);
  void addToOrder(unsigned index, unsigned type);

  /**
   * Creates an user friendly XML string from a given element with
   * appropriate white space and carriage returns.
   *
   * This recurses through all subnodes then adds contents of the
   * nodes to the string.
   */
  static void serialiseR(const XMLNodeData *pEntry, TextWriter &writer,
                         int nFormat);
  static const void *enumContent(const XMLNodeData *pEntry, unsigned i,
                                 XMLElementType *nodeType);

  gcc_pure
  static unsigned nElement(const XMLNodeData *pEntry);

  /**
   * Update "order" information when deleting a content of a XMLNode.
   */
  static void removeOrderElement(XMLNodeData *d, XMLElementType t,
                                 unsigned index);
} XMLNode;

// This structure is given by the function "enumContents".
typedef struct XMLNodeContents
{
  // This dictates what's the content of the XMLNodeContent
  enum XMLElementType type;
  // should be an union to access the appropriate data.
  // compiler does not allow union of object with constructor... too bad.
  XMLNode child;
  XMLAttribute attrib;
  LPCTSTR text;
} XMLNodeContents;

/**
 * Duplicate (copy in a new allocated buffer) the source string.
 */
LPTSTR stringDup(LPCTSTR source, size_t cbData = 0);

#endif
