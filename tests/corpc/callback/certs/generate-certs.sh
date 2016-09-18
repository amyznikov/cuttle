#! /bin/bash

# See:
# http://stackoverflow.com/questions/10175812/how-to-create-a-self-signed-certificate-with-openssl
# https://www.madboa.com/geek/openssl/#how-do-i-generate-a-self-signed-certificate
# https://habrahabr.ru/post/192446/ 
# http://stackoverflow.com/questions/10175812/how-to-create-a-self-signed-certificate-with-openssl
#
# -subj "/C=US/ST=Oregon/L=Portland/O=Company Name/OU=Org/CN=www.example.com"
#
# https://www.guyrutenberg.com/2013/12/28/creating-self-signed-ecdsa-ssl-certificate-using-openssl/
#



function genRootCA() {
	keytype="$1"
	keyname="$2"
	certname="$3"
	cn="$4"
		
	if [[ "$keytype" == "rsa" ]] ; then
		openssl genrsa -out ${keyname} 2048 || exit 1
	elif [[ $keytype == "ec" ]] ; then 
		openssl ecparam -genkey -name prime256v1 -out ${keyname} -param_enc named_curve || exit 1
	else
		echo "Invalid key type '$keytype' specified" 1>&2
		exit 1;
	fi
		
	# https://www.erianna.com/ecdsa-certificate-authorities-and-certificates-with-openssl
	openssl req \
		-x509 \
		-new \
		-sha256	\
		-nodes \
		-key ${keyname} \
		-days 365 \
		-out ${certname} \
		-subj "/C=UA/ST=HOME/L=ROOM/O=MYORG/OU=ROOTUNIT/CN=${cn}" || exit 1
}

function gencert() {
	keytype="$1"
	keyname="$2"
	certname="$3"
	cn="$4"
	rootcakey="$5"
	rootca="$6"
		
		
	if [[ "$keytype" == "rsa" ]] ; then
	openssl genrsa -out ${keyname} 2048 || exit 1
	elif [[ $keytype == "ec" ]] ; then 
		openssl ecparam -genkey -name prime256v1 -out ${keyname} -param_enc named_curve || exit 1
	else
		echo "Invalid key type '$keytype' specified" 1>&2
		exit 1;
	fi
	
	openssl req \
		-new -key ${keyname} -out ${keyname}.csr \
		-sha256 \
		-subj "/C=UA/ST=HOME/L=ROOM/O=MYORG/OU=MYORGUNIT/CN=${cn}"

	openssl x509 -req \
		-in ${keyname}.csr \
		-CA ${rootca} \
		-CAkey ${rootcakey} \
		-set_serial 125 \
		-out ${certname} \
		-days 365 \
		-sha256

#		-CAcreateserial 
}






domain=localhost
keytype=rsa

# Create root CA key
genRootCA ${keytype} ca.key ca.crt "${domain}" || exit 1

# Create server cert
gencert ${keytype} server.key server.crt "${domain}" ca.key ca.crt || exit 1

# Create test clients
for c in "client1" "client2" ; do
	gencert ${keytype} ${c}.key ${c}.crt ${c} ca.key ca.crt || exit 1
done
