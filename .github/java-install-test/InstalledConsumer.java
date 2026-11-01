import comtrade.ComtradeRecord;
import comtrade.ComtradeStreamReader;
import java.net.ServerSocket;
import java.net.Socket;
import java.io.ByteArrayOutputStream;
import java.net.InetSocketAddress;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.lang.reflect.InvocationTargetException;
import java.util.Comparator;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/** Compiled against the installed JAR only; no generated proxies or build-tree libraries. */
public final class InstalledConsumer {
    private static void require(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }
    private static Object call(Class<?> api, String name, Class<?>[] types, Object... args) throws Exception {
        try { return api.getMethod(name, types).invoke(null, args); }
        catch (InvocationTargetException e) {
            if (e.getCause() instanceof Exception) throw (Exception)e.getCause();
            throw e;
        }
    }
    private static String compact(String json) { return json.replaceAll("\\s", ""); }
    private static void core(Path dir) throws Exception {
        for (ComtradeRecord.DataType type : ComtradeRecord.DataType.values()) {
            Path cfg=dir.resolve(type+".cfg"), dat=dir.resolve(type+".dat"), cff=dir.resolve(type+".cff");
            try (ComtradeRecord r=new ComtradeRecord()) {
                r.setStationAndDevice("Installed station", "Installed relay", ComtradeRecord.StandardVersion.V2013);
                r.setDataType(type); r.set2013TimeMetadata("+0", "+0", "0", 0);
                r.setTimestamps("01/01/2020,00:00:00.000000000", "01/01/2020,00:00:00.001000000");
                r.addAnalogChannel(1,"IA","A","A",2,1);
                r.addDigitalChannel(1,"TRIP","",false);
                r.addSampleRate(new ComtradeRecord.SampleRate(1000,3));
                for(int i=0;i<3;++i) r.addSample(i*1000,new double[]{3+2*i},new boolean[]{i>0});
                r.setHeader("Installed header"); r.setInformation("[installed]\nverified=true");
                r.save(cfg.toString(),dat.toString()); r.saveCff(cff.toString());
            }
            try (ComtradeRecord r=new ComtradeRecord()) {
                r.load(cfg.toString(),dat.toString());
                require(r.getSampleCount()==3,"Installed DAT count: "+type);
                require(r.getAnalogValue(0,2)==7,"Installed DAT scaling: "+type);
                r.loadCff(cff.toString());
                require(r.getSampleCount()==3 && r.getHeader().contains("Installed header"),"Installed CFF: "+type);
            }
            try (ComtradeStreamReader reader=new ComtradeStreamReader(cff.toString())) {
                AtomicInteger count=new AtomicInteger();
                require(reader.processCffStream(row->{
                    int i=count.getAndIncrement();
                    require(row.analogValues[0]==3+2*i,"Installed stream values");
                    require(row.timeOffsetNanoseconds==i*1000000L,"Installed stream time");
                })==3 && count.get()==3,"Installed stream count");
            }
        }
    }
    private static void ai(Class<?> api, Path dir, String font) throws Exception {
        Class<?>[] triple={String.class,String.class,String.class};
        String summary=(String)call(api,"summarize",triple,dir.resolve("FLOAT32.cfg").toString(),dir.resolve("FLOAT32.dat").toString(),"{}");
        require(compact(summary).contains("\"samples\":3") && compact(summary).contains("\"mean\":5.0"),"Installed AI statistics");
        String full=(String)call(api,"summarizeCff",new Class<?>[]{String.class,String.class},dir.resolve("FLOAT32.cff").toString(),"{\"waveformMode\":\"full\"}");
        require(compact(full).contains("\"rows\":[") && compact(full).contains("\"samples\":3"),"Installed AI CFF/full evidence");
        AtomicInteger requests=new AtomicInteger(); AtomicReference<Throwable> serverError=new AtomicReference<>();
        ServerSocket server=new ServerSocket();
        server.bind(new InetSocketAddress("127.0.0.1",0)); server.setSoTimeout(15000);
        Thread responder=new Thread(()->{
            try(Socket socket=server.accept()) {
                socket.setSoTimeout(10000);
                ByteArrayOutputStream header=new ByteArrayOutputStream();
                String headers="";
                while(header.size()<65536) {
                    int next=socket.getInputStream().read();
                    require(next!=-1,"Incomplete HTTP request"); header.write(next);
                    if(next==10) {
                        headers=header.toString(StandardCharsets.US_ASCII.name());
                        if(headers.endsWith("\r\n\r\n")) break;
                    }
                }
                require(headers.startsWith("POST /v1/chat/completions "),"Installed AI HTTP method/path");
                int length=-1;
                for(String line:headers.split("\r\n")) if(line.toLowerCase(java.util.Locale.ROOT).startsWith("content-length:"))
                    length=Integer.parseInt(line.substring(line.indexOf(':')+1).trim());
                require(length>=0 && length<1000000,"Installed AI HTTP content length");
                byte[] request=socket.getInputStream().readNBytes(length);
                require(request.length==length,"Truncated AI request");
                String body=new String(request,StandardCharsets.UTF_8);
                require(body.contains("messages") && body.contains("installed-test"),"Installed AI request");
                requests.incrementAndGet();
                byte[] response="{\"choices\":[{\"finish_reason\":\"stop\",\"message\":{\"role\":\"assistant\",\"content\":\"安装后模型调用测试\"}}]}".getBytes(StandardCharsets.UTF_8);
                String responseHeader="HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "+response.length+"\r\nConnection: close\r\n\r\n";
                socket.getOutputStream().write(responseHeader.getBytes(StandardCharsets.US_ASCII));
                socket.getOutputStream().write(response); socket.getOutputStream().flush();
            } catch(Throwable error) { serverError.set(error); }
        },"installed-ai-http-test");
        responder.setDaemon(true); responder.start();
        String interpretation;
        try {
            String config="{\"endpoint\":\"http://127.0.0.1:"+server.getLocalPort()+"/v1/chat/completions\",\"model\":\"installed-test\",\"allowInsecureLocalhost\":true}";
            interpretation=(String)call(api,"analyze",new Class<?>[]{String.class,String.class},summary,config);
            require(interpretation.equals("安装后模型调用测试"),"Installed AI model response");
            require(requests.get()==1 && serverError.get()==null,"Installed AI HTTP exchange");
        } finally { server.close(); responder.join(5000); }
        require(!responder.isAlive(),"HTTP responder did not terminate");
        Path pdf=dir.resolve("installed-report.pdf");
        String pdfConfig="{\"pdfFontPath\":\""+font.replace("\\","/").replace("\"","\\\"")+"\"}";
        call(api,"exportPdf",new Class<?>[]{String.class,String.class,String.class,String.class},pdf.toString(),summary,interpretation,pdfConfig);
        byte[] bytes=Files.readAllBytes(pdf);
        require(bytes.length>1000 && new String(bytes,0,5,StandardCharsets.US_ASCII).equals("%PDF-"),"Installed AI PDF rendering");
    }
    public static void main(String[] args) throws Exception {
        Path installed=Paths.get(args[0]).toRealPath();
        Path jar=Paths.get(ComtradeRecord.class.getProtectionDomain().getCodeSource().getLocation().toURI()).toRealPath();
        require(jar.equals(installed.resolve("comtrade-core-java.jar").toRealPath()),"Consumer did not load the installed JAR");
        require(Paths.get(System.getProperty("java.library.path")).toRealPath().equals(installed),"JNI search path is not the install directory");
        boolean expectAi=args[1].equals("ON");
        Class<?> api=null;
        try { api=Class.forName("comtrade.ComtradeAI"); } catch(ClassNotFoundException coreOnly) {}
        require((api!=null)==expectAi,"Installed AI API presence differs from requested build mode");
        Path dir=Files.createTempDirectory("comtrade-installed-java-");
        try { core(dir); if(expectAi) ai(api,dir,args[2]); }
        finally {
            try(java.util.stream.Stream<Path> files=Files.walk(dir)) {
                for(Path path:(Iterable<Path>)files.sorted(Comparator.reverseOrder())::iterator) Files.delete(path);
            }
        }
        System.out.println("Installed Java "+(expectAi?"core + AI (HTTP/PDF)":"core")+" consumer passed");
    }
}
