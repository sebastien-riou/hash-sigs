/*
 * This is a demonstration program for hss
 *
 * It is a simple file sign/verify utility: it is used as follows:
 *
 *   demo genkey keyname
 *       This creates a public/private key; it places the private key into
 *       keyname.prv, it places the public key into keyname.pub, and it places
 *       the auxiliary data into keyname.aux
 *   demo genkey keyname  15/4,10/8:2000
 *       This does the same, but specifies the parmaeter set to use.  In this
 *       example, it states that we have two Merkle levels, the top has 15
 *       levels (and uses Winternitz parameter 4), the bottom has 10 levels
 *       (and uses Winternitz parmaeter 8); up to 2000 bytes of aux data are
 *       used.  If you don't include the /x (Winternitz parameter) or the
 *       :2000 (aux data size), reasonable defaults are used
 *   demo sign keyname file.1 file.2 ... file.n
 *       This loads the private key keyname.prv (using keyname.aux if present)
 *       and then signs the files, producing the detached signatures
 *       file.1.sig, file.2.sig, ..., file.n.sig
 *       It also updates the keyname.prv file to reflect the generated
 *       signatures
 *   demo verify keyname file.1 file.2 ... file.n
 *       This takes the public key in keyname.pub, and uses it to verify
 *       whether file.1.sig is a valid signature for file.1, file.2.sig is
 *       a valid signature for file.2, etc
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include "hss.h"
#include "hss_verify_inc.h"
#include "hss_sign_inc.h"

   /* When we generate a key, these are the parameters we use (unless the */
   /* user has specified otherwise). For signature generation/verification, */
   /* we use the parameters from the private key (for signature */
   /* generation) or the public key and signature (for verification) */

   /* By default, we use Merkle trees with two levels */
   /* Top tree has 20 levels, bottom tree has 10 (and so it's 20/10 in the */
   /* notation we use elsewhere) */
   /* We use Winternitz 8 for both trees; this minimizes the signature size */
   /* This gives us a reasonable genkey time (3 minutes with threading), */
   /* good load times (perhaps 1 second), and a billion signatures per key */
const char *default_parm_set = "20/8,10/8";

#define DEFAULT_AUX_DATA 8740   /* Use 8+k of aux data (which works well */
                            /* with the above default parameter set) */

/*
 * The HSS routines assume 3 user provided routines; here are the ones
 * this demo routine provides
 */

/*
 * This is a function that is supposed to generate truly random values.
 * This is a hideous version of this; this needs to be replaced by something
 * secure in a real product
 */
#include "hash.h"
#include "hss_zeroize.h"
bool do_rand( void *output, size_t len ) {
    struct {
        unsigned char dev_random_output[32];
        int rand_output[16];
        /* Potentially more random sources here */
        unsigned count;
    } buffer;
    int i;

    /* Try to grab a sammple of /dev/urandom output */
    /* We use /dev/urandom because there's no point in blocking; this is a */
    /* demo program */
    FILE *f = fopen( "/dev/urandom", "r" );
    if (f) {
         (void)fread( buffer.dev_random_output, 1, 32, f );
         fclose(f);
    }

    /* Also try to grab some output from rand */
    /* It's not great, but if the /dev/urandom output fails, at least we */
    /* have something */
    /* In a real program, we'd want to fail if we don't have enough */
    /* entropy, but hey, this is a demo */
    static int set_seed = 0;
    if (!set_seed) {
        srand( time(0) );
        set_seed = 1;
    }
    for (i = 0; i<16; i++) {
        buffer.rand_output[i] = rand();
    }


    /* If we had more random sources, we'd sample them here */

    unsigned output_buffer[32];
    for (i=0; len>0; i++) {
        buffer.count = i;

        /* Ok, hash all our random samples together to generate the random */
        /* string that was asked for */
        hss_hash( output_buffer, HASH_SHA256, &buffer, sizeof buffer );

        /* Copy that hash to the output buffer */
        int this_len = 32;
        if (this_len > len) this_len = len;
        memcpy( output, output_buffer, this_len );

        /* Advance pointers */
        output = (unsigned char *)output + this_len; len -= this_len;
    }

    /* Clean up after ourselves.  Yes, this is a demo program; doesn't mean */
    /* we get to be sloppy */
    hss_zeroize( output_buffer, sizeof output_buffer );
    hss_zeroize( &buffer, sizeof buffer );

    return true;
}

/*
 * This saves the private key to secure storage; in this case, a file on the
 * filesystem.  The context pointer we use here is the filename
 */
static bool update_private_key( unsigned char *private_key,
                               size_t len_private_key, void *filename) {
    FILE *f = fopen( filename, "r+" );
    if (!f) {
        /* Open failed, possibly because the file didn't exist */
        f = fopen( filename, "w" );
        if (!f) {
            /* Unable to open file */
            return false;
        }
    }
    if (1 != fwrite( private_key, len_private_key, 1, f )) {
        /* Write failed */
        fclose(f);
        return false;
    }
    if (0 != fclose(f)) {
        /* Close failed (possibly because pending write failed) */
        return false;
    }

    /* Everything succeeded */
    return true;
}

/*
 * This retrieves the private key from secure storage; in this case, a file on
 * the filesystem.  The context pointer we use here is the filename
 */
static bool read_private_key( unsigned char *private_key,
                              size_t len_private_key, void *filename) {
    FILE *f = fopen( filename, "r" );
    if (!f) {
        return false;
    }
    if (1 != fread( private_key, len_private_key, 1, f )) {
        /* Read failed */
        fclose(f);
        return false;
    }
    fclose(f);

    /* Everything succeeded */
    return true;
}

/*
 * This will read in the file into a malloc'ed area
 * The hss routines assume that everything public keys, auxilary data and
 * signatures are in contiguous memory; this is used to read them in.
 *
 * This isn't used to read in the files being signed/verified; we read
 * those in chunks within the sign()/verify() routines below.
 */
void *read_file( const char *filename, size_t *len ) {
    FILE *f = fopen( filename, "r" );
    if (!f) return 0;

#define FILE_INCREMENT 20000
    unsigned alloc_len = FILE_INCREMENT;
    unsigned char *p = malloc( alloc_len );
    if (!p) return 0;

    unsigned cur_len = 0;
    for (;;) {
        unsigned delta = alloc_len - cur_len;
        if (delta == 0) {
            unsigned char *q = realloc( p, alloc_len + FILE_INCREMENT );
            if (!q) {
                free(p);
                return 0;
            }
            p = q;
            alloc_len += FILE_INCREMENT;
            delta = FILE_INCREMENT;
        }
        int n = fread( p + cur_len, 1, delta, f );
        if (n <= 0) break;
        cur_len += n;
    }

    if (len) *len = cur_len;
    return p;
}

static int parse_parm_set(int *levels, param_set_t *lm_array,
                           param_set_t *ots_array, size_t *aux_size,
                           const char *parm_set);
static void list_parameter_set(int levels, const param_set_t *lm_array,
                           const param_set_t *ots_array, size_t aux_size );

/*
 * This function implements the 'genkey' command
 *
 * It generates the key, and writes the private_key, public key and the aux
 * data to disk.  The private key is also written to disk by the
 * update_private_key function; we write out the public key and the aux data
 * explicitly
 *
 * With the default parameters, this takes quite a while if we're not
 * in threaded mode; in threaded mode, it takes 3 minutes on my test
 * equipment
 */ 
static int keygen(const char *keyname, const char *parm_set) {

    /* Parse the parameter set */
    int levels;
    param_set_t lm_array[ MAX_HSS_LEVELS ];
    param_set_t ots_array[ MAX_HSS_LEVELS ];
    size_t aux_size;
    if (!parm_set) parm_set = default_parm_set;
    if (!parse_parm_set( &levels, lm_array, ots_array, &aux_size, parm_set)) {
        return 0;
    }

    /* Tell the user how we interpreted the parameter set he gave us */
    list_parameter_set( levels, lm_array, ots_array, aux_size );

    /* We'll place the private key here */
     char private_key_filename[ strlen(keyname) + sizeof (".prv" ) + 1 ];
     sprintf( private_key_filename, "%s.prv", keyname );

    /* We'll place the public key in this array */
    unsigned len_public_key = hss_get_public_key_len(levels,
                                                lm_array, ots_array);
    if (len_public_key == 0) return 0;
    unsigned char public_key[len_public_key];

    /* And we'll place the aux data in this array */
    unsigned aux_len;
    if (aux_size > 0) {
        aux_len = hss_get_aux_data_len( aux_size, levels,
                                               lm_array, ots_array);
        printf( "aux_len = %d\n", aux_len );
    } else {
        aux_len = 1;
    }
    unsigned char aux[aux_len];

    printf( "Generating public key %s (will take a while)\n",
                                       private_key_filename );
    if (!hss_generate_private_key(
        do_rand,       /* Routine to generate randomness */
        levels,        /* # of Merkle levels */
        lm_array, ots_array,  /* The LM and OTS parameters */
        update_private_key, private_key_filename, /* Routine to write out */
                                       /* the genearted private key */
        public_key, len_public_key,  /* The public key is placed here */
        aux_size > 0 ? aux : 0, aux_len, /* Where to place the aux data */
        0)) {            /* Use the defaults for extra info */
            return 0;
    }

    char public_key_filename[ strlen(keyname) + sizeof (".pub" ) + 1 ];
    sprintf( public_key_filename, "%s.pub", keyname );

    printf( "Success!\nWriting public key %s\n", public_key_filename );
    FILE *f = fopen( public_key_filename, "w" );
    if (!f) {
        fprintf( stderr, "Error: unable to write public key\n" );
        return 0;
    }
    if (1 != fwrite( public_key, len_public_key, 1, f )) {
        /* Write failed */
        fclose(f);
        return 0;
    }
    if (0 != fclose(f)) {
        fprintf( stderr, "Error: unable to close public key file\n" );
        /* Close failed (possibly because pending write failed) */
        return 0;
    }

    if (aux_size > 0) {
        char aux_filename[ strlen(keyname) + sizeof (".aux" ) + 1 ];
        sprintf( aux_filename, "%s.aux", keyname );

        /* Attempt to write the aux file.  Note that if we fail, we'll still */
        /* claim to have succeeded (as the aux file is optional) */
        printf( "Writing aux data %s\n", aux_filename );
        f = fopen( aux_filename, "w" );
        if (!f) {
            fprintf( stderr, "Warning: unable to write aux file\n" );
            return 1;
        }
        if (1 != fwrite( aux, aux_len, 1, f )) {
            fprintf( stderr, "Warning: unable to write aux file\n" );
            /* Write failed */
            fclose(f);
            return 1;
        }
        if (0 != fclose(f)) {
            fprintf( stderr, "Warning: close failed writing aux file\n" );
            /* Close failed (possibly because pending write failed) */
            return 1;
        }
    }

    return 1;
}

/*
 * This function implements the 'sign' command; it loads the private key, and
 * then for each file, loads it into memory, generates the signature, and
 * writes the signature out to disk
 */
static int sign(const char *keyname, char **files) {
    char private_key_filename[ strlen(keyname) + sizeof (".prv" ) + 1 ];
    sprintf( private_key_filename, "%s.prv", keyname );

        /* Read in the auxilliary file */   
    char aux_filename[ strlen(keyname) + sizeof (".aux" ) + 1 ];
    sprintf( aux_filename, "%s.aux", keyname );
    size_t len_aux_data = 0;
    void *aux_data = read_file( aux_filename, &len_aux_data );
    if (aux_data != 0) {
        printf( "Processing with aux data\n" );
    } else {
        /* We don't have the aux data; proceed without it */
        printf( "Processing without aux data\n" );
    }

        /* Load the working key into memory */
    printf( "Loading private key\n" );
    fflush(stdout);
    struct hss_working_key *w = hss_load_private_key(
             read_private_key, private_key_filename, /* How to load the */
                                         /* private key */
             0,                          /* Use minimal memory */
             aux_data, len_aux_data,     /* The auxiliary data */
             0);                         /* Use the defaults for extra info */
    if (!w) {
        printf( "Error loading private key\n" );
        free(aux_data);
        hss_free_working_key(w);
        return 0;
    }
    free(aux_data);

    printf( "Loaded private key\n" );  /* printf here mostly so the user */
    fflush(stdout);              /* gets a feel for how long this step took */
                                 /* compared to the signing steps below */

    /* Now, go through the file list, and generate the signatures for each */

    /* Look up the signature length */
    size_t sig_len;
    sig_len = hss_get_signature_len_from_working_key(w);
    if (sig_len == 0) {
        printf( "Error getting signature len\n" );
        hss_free_working_key(w);
        return 0;
    }

    unsigned char sig[sig_len];
    int i;
    for (i=0; files[i]; i++) {
        printf( "Signing %s\n", files[i] );

        /*
         * Read the file in, and generate the signature.  We don't want to
         * assume that we can fit the entire file into memory, and so we
         * read it in in pieces, and use the API that allows us to sign
         * the message when given in pieces
         */
        FILE *f = fopen( files[i], "r" );
        if (!f) {
            printf( "    %s: unable to read\n", files[i] );
            continue;
        }

        struct hss_sign_inc ctx;
        (void)hss_sign_init(
             &ctx,                 /* Incremental signing context */
             w,                    /* Working key */
             update_private_key,    /* Routine to update the */
             private_key_filename, /* private key */
             sig, sig_len,         /* Where to place the signature */
             0);                   /* Use the defaults for extra info */

        char buffer[1024];
        for (;;) {
            int n = fread( buffer, 1, sizeof buffer, f );
            if (n <= 0) break;
            (void)hss_sign_update(
                &ctx,           /* Incremental signing context */
                buffer,         /* Next piece of the message */
                n);             /* Length of this piece */
        }
        fclose(f);

        bool status = hss_sign_finalize(
             &ctx,               /* Incremental signing context */
             w,                  /* Working key */
             sig,                /* Signature */
             0);                 /* Use the defaults for extra info */

        if (!status) {
            printf( "    Unable to generate signature\n" );
            continue;
        }

        char sig_file_name[ strlen(files[i]) + sizeof( ".sig" ) + 1 ];
        sprintf( sig_file_name, "%s.sig", files[i] );
        f = fopen( sig_file_name, "w" );
        if (!f) {
            printf( "    %s: unable to create\n", sig_file_name );
            continue;
        }
        if (1 != fwrite( sig, sig_len, 1, f )) {
            printf( "    %s: unable to write\n", sig_file_name );
            fclose(f);
            continue;
        }
        fclose(f);
        printf( "    signed (%s)\n", sig_file_name );
    }

    hss_free_working_key(w);
    return 1;
}

/*
 * This function implements the 'verify' command; this reads the public key,
 * and then for each file, reads the file and the signature from disk, and
 * attempts to verify the signature
 * It verifies each file incrementally, and so we don't need to assume the
 * file is short enough to fit into memory
 */
static int verify(const char *keyname, char **files) {
    /* Step 1: read in the public key */
    char public_key_filename[ strlen(keyname) + sizeof ".pub" + 1 ];
    sprintf( public_key_filename, "%s.pub", keyname );
    unsigned char *pub = read_file( public_key_filename, 0 );
    if (!pub) {
         printf( "Error: unable to read %s\n", public_key_filename );
         return 0;
    }
    int i;
    for (i=0; files[i]; i++) {
        printf( "Verifying %s\n", files[i] );

            /* Read in the signatre */
        char sig_file_name[ strlen(files[i]) + sizeof( ".sig" ) + 1 ];
        sprintf( sig_file_name, "%s.sig", files[i] );
        size_t sig_len;
        void *sig = read_file( sig_file_name, &sig_len );
        if (!sig) {
            printf( "    %s: unable to read signature file %s\n", files[i], sig_file_name );
            continue;
        }

        /*
         * Read the file in, and verify the signature.  We don't want to
         * assume that we can fit the entire file into memory, and so we
         * read it in in pieces, and use the API that allows us to verify
         * the message when given in pieces
         */
        FILE *f = fopen( files[i], "r" );
        if (!f) {
            printf( "    %s: unable to read\n", files[i] );
            free(sig);
            continue;
        }

        struct hss_validate_inc ctx;
        (void)hss_validate_signature_init(
             &ctx,               /* Incremental validate context */
             pub,                /* Public key */
             sig, sig_len,       /* Signature */
             0);                 /* Use the defaults for extra info */

        char buffer[1024];
        for (;;) {
            int n = fread( buffer, 1, sizeof buffer, f );
            if (n <= 0) break;
            (void)hss_validate_signature_update(
                &ctx,           /* Incremental validate context */
                buffer,         /* Next piece of the message */
                n);             /* Length of this piece */
        }
        fclose(f);

        bool status = hss_validate_signature_finalize(
             &ctx,               /* Incremental validate context */
             sig,                /* Signature */
             0);                 /* Use the defaults for extra info */

        free(sig);

        if (status) {
            printf( "    Signature verified\n" );
        } else {
            printf( "    Signature NOT verified\n" );
        }
    }

    return 1;
}

static void usage(char *program) {
    printf( "Usage:\n" );
    printf( " %s genkey [keyname]\n", program );
    printf( " %s genkey [keyname] [parameter set]\n", program );
    printf( " %s sign [keyname] [files to sign]\n", program );
    printf( " %s verify [keyname] [files to verify]\n", program );
}

static int get_integer(const char **p) {
    int n = 0;

    while (isdigit(**p)) {
        n = 10*n + **p - '0';
        *p += 1;
    }

    return n;
}

/*
 * This parses the parameter set; this is provided so we can try different
 * sets without recompiling the program each time.  This is placed here
 * because it's ugly parsing code that has nothing to do with how to use
 * HSS
 */
static int parse_parm_set( int *levels, param_set_t *lm_array,
                           param_set_t *ots_array, size_t *aux_size,
                           const char *parm_set) {
    int i;
    size_t aux = DEFAULT_AUX_DATA;
    for (i=0;; i++) {
        if (i == 8) {
            printf( "Error: more than 8 HSS levels specified\n" );
            return 0;
        }
        /* Get the number of levels of this tree */
        int h = get_integer( &parm_set );
        param_set_t lm;
        switch (h) {
        case 5:  lm = LMS_SHA256_N32_H5;  break;
        case 10: lm = LMS_SHA256_N32_H10; break;
        case 15: lm = LMS_SHA256_N32_H15; break;
        case 20: lm = LMS_SHA256_N32_H20; break;
        case 25: lm = LMS_SHA256_N32_H25; break;
        case 0: printf( "Error: expected height of Merkle tree\n" ); return 0;
        default: printf( "Error: unsupported Merkle tree height %d\n", h );
                 printf( "Supported heights = 5, 10, 15, 20, 25\n" );
                 return 0;
        }
        /* Now see if we can get the Winternitz parameter */
        param_set_t ots = LMOTS_SHA256_N32_W8;
        if (*parm_set == '/') {
            parm_set++;
            int w = get_integer( &parm_set );
            switch (w) {
            case 1: ots = LMOTS_SHA256_N32_W1; break;
            case 2: ots = LMOTS_SHA256_N32_W2; break;
            case 4: ots = LMOTS_SHA256_N32_W4; break;
            case 8: ots = LMOTS_SHA256_N32_W8; break;
            case 0: printf( "Error: expected Winternitz parameter\n" ); return 0;
            default: printf( "Error: unsupported Winternitz parameter %d\n", w );
                     printf( "Supported parmaeters = 1, 2, 4, 8\n" );
                     return 0;
            }
        }

        lm_array[i] = lm;
        ots_array[i] = ots;

        if (*parm_set == ':') {
            parm_set++;
            aux = get_integer( &parm_set );
            break;
        }
        if (*parm_set == '\0') break;
        if (*parm_set == ',') { parm_set++; continue; }
        printf( "Error: parse error after tree specification\n" ); return 0;
    }

    *levels = i+1;
    *aux_size = aux;
    return 1;
}

static void list_parameter_set(int levels, const param_set_t *lm_array,
                           const param_set_t *ots_array, size_t aux_size ) {
    printf( "Parameter set being used: there are %d levels of Merkle trees\n", levels );
    int i;
    for (i=0; i<levels; i++) {
        printf( "Level %d: hash function = SHA-256; ", i );
        int h = 0;
        switch (lm_array[i]) {
        case LMS_SHA256_N32_H5:  h = 5; break;
        case LMS_SHA256_N32_H10: h = 10; break;
        case LMS_SHA256_N32_H15: h = 15; break;
        case LMS_SHA256_N32_H20: h = 20; break;
        case LMS_SHA256_N32_H25: h = 25; break;
        }
        printf( "%d level Merkle tree; ", h );
        int w = 0;
        switch (ots_array[i]) {
        case LMOTS_SHA256_N32_W1: w = 1; break;
        case LMOTS_SHA256_N32_W2: w = 2; break;
        case LMOTS_SHA256_N32_W4: w = 4; break;
        case LMOTS_SHA256_N32_W8: w = 8; break;
        }
        printf( "Winternitz param %d\n", w );
    }
    if (aux_size > 0) {
        printf( "Maximum of %lu bytes of aux data\n", (unsigned long)aux_size );
    } else {
        printf( "Aux data disabled\n" );
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 0;
    }
    if (0 == strcmp( argv[1], "genkey" )) {
        if (argc < 3) {
            printf( "Error: mssing keyname argument\n" );
            usage(argv[0]);
            return 0;
        }
        if (argc > 4) {
            printf( "Error: unexpected argument after parmset\n" );
            usage(argv[0]);
            return 0;
        }

        if (!keygen( argv[2], argv[3] )) {
            printf( "Error creating keys\n" );
        }
        return 0;
    }
    if (0 == strcmp( argv[1], "sign" )) {
        if (argc < 4) {
            printf( "Error: mssing keyname and file argument\n" );
            usage(argv[0]);
            return 0;
        }
        if (!sign( argv[2], &argv[3] )) {
            printf( "Error signing\n" );
        }
        return 0;
    }
    if (0 == strcmp( argv[1], "verify" )) {
        if (argc < 4) {
            printf( "Error: mssing keyname and file argument\n" );
            usage(argv[0]);
            return 0;
        }
        if (!verify( argv[2], &argv[3] )) {
            printf( "Error verifying\n" );
        }
        return 0;
    }

    usage(argv[0]);
    return 0;
}