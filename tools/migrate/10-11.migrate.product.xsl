<!--
  Code10 to Code11 conversion of product data.

    Code10: /var/lib/zypp/db/products/
    Code11: /etc/products.d

  use: xsltproc convert.xsl $OLDFILE >$NEWFILE
-->
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
xmlns:zypp="http://www.novell.com/metadata/zypp/xml-store">

  <xsl:output omit-xml-declaration="yes" indent="yes"/>
  <xsl:strip-space elements="*"/>

  <!-- declare some variables to build the product id and target -->
  <xsl:variable name="product-id">
          <xsl:text>sle-</xsl:text>
          <xsl:choose>
            <xsl:when test="contains(/zypp:product/zypp:version/@ver, '10')"><xsl:text>10</xsl:text></xsl:when>
            <xsl:when test="contains(/zypp:product/zypp:version/@ver, '11')"><xsl:text>11</xsl:text></xsl:when>
          </xsl:choose>
          <xsl:text>-</xsl:text>
          <xsl:choose>
            <xsl:when test="contains(/zypp:product/zypp:name, 'SLES')"><xsl:text>server</xsl:text></xsl:when>
            <xsl:when test="contains(/zypp:product/zypp:name, 'SLED')"><xsl:text>desktop</xsl:text></xsl:when>
          </xsl:choose>
  </xsl:variable>

  <xsl:variable name="product-target">
          <xsl:choose>
            <xsl:when test="contains(/zypp:product/zypp:name, 'SLES')"><xsl:text>sles</xsl:text></xsl:when>
            <xsl:when test="contains(/zypp:product/zypp:name, 'SLED')"><xsl:text>sled</xsl:text></xsl:when>
          </xsl:choose>
          <xsl:text>-</xsl:text>
          <xsl:choose>
            <xsl:when test="contains(/zypp:product/zypp:version/@ver, '10')"><xsl:text>10</xsl:text></xsl:when>
            <xsl:when test="contains(/zypp:product/zypp:version/@ver, '11')"><xsl:text>11</xsl:text></xsl:when>
          </xsl:choose>
          <xsl:text>-</xsl:text>
          <xsl:value-of select="/zypp:product/zypp:arch"/>
  </xsl:variable>

  <!-- copy all elements without the namespace -->
  <xsl:template match="*">
    <xsl:element name="{local-name()}">
      <xsl:apply-templates select="@*|node()"/>
    </xsl:element>
  </xsl:template>

  <!-- the rest just as is -->
  <xsl:template match="@*|text()|comment()|processing-instruction()">
    <xsl:copy>
      <xsl:apply-templates select="@*|node()"/>
    </xsl:copy>
  </xsl:template>

  <!-- when copying product, ad the id and other attributes -->
  <xsl:template match="zypp:product">
    <xsl:element name="product">
      <xsl:attribute name="id">
        <xsl:copy-of select="$product-id" />
      </xsl:attribute>
      <xsl:attribute name="schemeversion">0</xsl:attribute>
      <xsl:apply-templates select="node() | @*"/>
    </xsl:element>
  </xsl:template>

  <xsl:template match="zypp:name">
    <xsl:element name="name">
      <xsl:value-of select="/zypp:product/zypp:distribution-name"/>
    </xsl:element>
  </xsl:template>


  <xsl:template match="zypp:version">
   <!-- product version goes as element -->
      <xsl:element name="version">
        <xsl:value-of select="substring-before(/zypp:product/zypp:distribution-edition, '-')"/>
      </xsl:element>
      <xsl:element name="release">
        <xsl:value-of select="substring-after(/zypp:product/zypp:distribution-edition, '-')"/>
      </xsl:element>

      <xsl:element name="baseversion">
        <xsl:value-of select="substring-before(/zypp:product/zypp:version/@ver,'.')"/>
      </xsl:element>
      <xsl:element name="patchlevel">
        <xsl:value-of select="substring-after(/zypp:product/zypp:version/@ver,'.')"/>

      </xsl:element>
      <xsl:element name="productline">sles</xsl:element>

      <xsl:element name="register">
        <xsl:element name="target">
          <xsl:copy-of select="$product-target" />
        </xsl:element>
        <xsl:element name="release">
          <xsl:value-of select="substring-after(/zypp:product/zypp:distribution-edition, '-')"/>
        </xsl:element>

        <xsl:element name="repositories"/>
      </xsl:element>
  </xsl:template>

  <!-- exclude -->
  <xsl:template match="zypp:product/@version"/>
  <xsl:template match="zypp:product/@type"/>
  <xsl:template match="zypp:description[@lang]"/>
  <xsl:template match="zypp:summary[@lang]"/>
  <xsl:template match="zypp:obsoletes"/>
  <xsl:template match="zypp:provides"/>
  <xsl:template match="zypp:requires"/>
  <xsl:template match="zypp:install-notify"/>
  <xsl:template match="zypp:delete-notify"/>
  <xsl:template match="zypp:size"/>
  <xsl:template match="zypp:distribution-edition"/>
  <xsl:template match="zypp:license-to-confirm"/>
  <xsl:template match="zypp:archive-size"/>
  <xsl:template match="zypp:install-only"/>
  <xsl:template match="zypp:build-time"/>
  <xsl:template match="zypp:install-time"/>
  <xsl:template match="zypp:shortname"/>
  <xsl:template match="zypp:distribution-name"/>
  <xsl:template match="zypp:source"/>
  <xsl:template match="zypp:release-notes-url"/>
  <xsl:template match="zypp:update-urls"/>
  <xsl:template match="zypp:extra-urls"/>
  <xsl:template match="zypp:optional-urls"/>
  <xsl:template match="zypp:product-flags"/>
</xsl:stylesheet>

