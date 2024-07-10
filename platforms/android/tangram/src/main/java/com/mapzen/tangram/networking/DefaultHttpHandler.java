package com.mapzen.tangram.networking;

import android.util.Log;

import java.io.IOException;
import java.net.InetAddress;
import java.net.Socket;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.security.cert.X509Certificate;
import java.util.concurrent.TimeUnit;

import javax.net.SocketFactory;
import javax.net.ssl.HostnameVerifier;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSession;
import javax.net.ssl.SSLSocketFactory;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

import com.mapzen.tangram.BuildConfig;

import okhttp3.Call;
import okhttp3.HttpUrl;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;
import okhttp3.ResponseBody;

/**
 * {@code DefaultHttpHandler} is an implementation of {@link HttpHandler} using OkHTTP.
 * Customize this class for your own application by subclassing and overriding
 * {@link DefaultHttpHandler#configureRequest(HttpUrl, Request.Builder)}.
 */
public class DefaultHttpHandler implements HttpHandler {

    private final OkHttpClient okClient;

    private static OkHttpClient.Builder getUnsafeOkHttpClientBuilder() {
        try {
            // Create a trust manager that does not validate certificate chains
            final TrustManager[] trustAllCerts = new TrustManager[]{
                    new X509TrustManager() {
                        @Override
                        public void checkClientTrusted(java.security.cert.X509Certificate[] chain,
                                                       String authType) {
                        }

                        @Override
                        public void checkServerTrusted(java.security.cert.X509Certificate[] chain,
                                                       String authType) {
                        }

                        @Override
                        public java.security.cert.X509Certificate[] getAcceptedIssuers() {
                            return new X509Certificate[0];
                        }
                    }
            };

            // Install the all-trusting trust manager
            final SSLContext sslContext = SSLContext.getInstance("SSL");
            sslContext.init(null, trustAllCerts, new java.security.SecureRandom());
            // Create an ssl socket factory with our all-trusting manager
            final SSLSocketFactory sslSocketFactory = sslContext.getSocketFactory();

            return new OkHttpClient.Builder()
                    .sslSocketFactory(sslSocketFactory, (X509TrustManager) trustAllCerts[0])
                    .hostnameVerifier(new HostnameVerifier() {
                        @Override
                        public boolean verify(String hostname, SSLSession session) {
                            return true;
                        }
                    });

        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }
    /**
     * @return OkHttp client builder with recommended settings for Tangram.
     */
    public static OkHttpClient.Builder getClientBuilder() {
        return getUnsafeOkHttpClientBuilder()
                .followRedirects(true)
                .followSslRedirects(true)
                .socketFactory(new CustomSocketFactory())
                .connectTimeout(10, TimeUnit.SECONDS)
                .writeTimeout(10, TimeUnit.SECONDS)
                .readTimeout(20, TimeUnit.SECONDS);
    }

    /**
     * Construct an {@code DefaultHttpHandler} with default options.
     * Uses an OkHttp client from {@link #getClientBuilder()}.
     */
    public DefaultHttpHandler() {
        this(getClientBuilder());
    }

    /**
     * Construct a {@code DefaultHttpHandler} with a custom OkHttp client builder.
     * In most cases you should start with a builder from {@link #getClientBuilder()}.
     * @param builder Custom OkHttp client builder
     */
    public DefaultHttpHandler(@NonNull final OkHttpClient.Builder builder) {
        configureClient(builder);
        okClient = builder.build();
    }

    @Keep
    @Override
    public Object startRequest(@NonNull final String url, @NonNull final HttpHandler.Callback cb) {
        final HttpUrl httpUrl = HttpUrl.parse(url);
        if (httpUrl == null) {
            cb.onFailure(new IOException("Failed to parse URL: " + url));
            return null;
        }
        // Construct okhttp3.Callback which forwards response calls to HttpResponse.Callback
        final okhttp3.Callback callback = new okhttp3.Callback() {
            @Override
            public void onFailure(final Call call, final IOException e) {
                if (call.isCanceled()) {
                    cb.onCancel();
                    return;
                }
                cb.onFailure(e);
            }

            @Override
            public void onResponse(@NonNull final Call call, final Response response) {
                byte[] data;
                final ResponseBody body = response.body();
                if (body != null) {
                    try {
                        data = body.bytes();
                        cb.onResponse(response.code(), data);
                    } catch (final IOException e) {
                        Log.e(BuildConfig.TAG, "Error reading bytes from response body:" + e.getMessage());
                        onFailure(call, e);
                    } finally {
                        response.close();
                    }
                }
            }
        };
        final Request.Builder builder = new Request.Builder().url(httpUrl);
        configureRequestBuilder(builder);
        // Some tile endpoints treat requests as "unauthorized" if they have no User-Agent header,
        // so add a default value to every request. Users can override this in configureRequest().
        builder.addHeader("User-Agent", "tangram");
        configureRequest(httpUrl, builder);
        final Request request = builder.build();
        Call call = okClient.newCall(request);
        call.enqueue(callback);
        return call;
    }

    @Keep
    protected HttpUrl addQueryParameter(HttpUrl url, String key, String value) {
        HttpUrl.Builder builder =  url.newBuilder();
        builder.setQueryParameter(key, value);
        return  builder.build();
    }

    @Keep
    protected void configureRequestBuilder(Request.Builder builder) {

    }

    @Override
    public void cancelRequest(final Object request) {
        if (request instanceof Call) {
            Call call = (Call)request;
            call.cancel();
        }
    }

    /**
     * Override this method to customize the OkHTTP client
     */
    @Keep
    protected void configureClient(OkHttpClient.Builder builder) {
    }

    /**
     * Override this method to customize HTTP requests
     * @param url Request URL
     * @param builder Request builder to customize
     */
    @Keep
    protected void configureRequest(HttpUrl url, Request.Builder builder) {
    }

    static class CustomSocketFactory extends SocketFactory {

        SocketFactory factory = SocketFactory.getDefault();

        @Override
        public Socket createSocket() throws IOException {
            Socket s = factory.createSocket();
            try {
                //Log.d("Tangram", "Patching socket nodelay: " + s.getTcpNoDelay());
                s.setTcpNoDelay(true);
            } catch (SocketException e) {
                // empty
            }
            return s;
        }

        @Override
        public Socket createSocket(String s, int i) throws IOException, UnknownHostException {
            return null;
        }

        @Override
        public Socket createSocket(String s, int i, InetAddress inetAddress, int i1) throws IOException, UnknownHostException {
            return null;
        }

        @Override
        public Socket createSocket(InetAddress inetAddress, int i) throws IOException {
            return null;
        }

        @Override
        public Socket createSocket(InetAddress inetAddress, int i, InetAddress inetAddress1, int i1) throws IOException {
            return null;
        }
    }
}
