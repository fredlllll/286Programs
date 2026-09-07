using HddSaver.Data;
using HddSaver.Protocol;
using Microsoft.EntityFrameworkCore;

namespace HddSaver.Services;

public static class BadMapGenerator
{
    private static readonly Dictionary<int, string> MapColors = new()
    {
        { 0, "#bdbdbd" }, // not attempted / missing
        { 1, "#2ea44f" }, // ok (0x00)
        { 2, "#a5d6a7" }, // ecc corrected (0x11), lighter green
        { 3, "#d93025" }, // hdd read failed
    };

    private static readonly Dictionary<int, string> MapNames = new()
    {
        { 0, "missing" },
        { 1, "ok" },
        { 2, "ecc corrected" },
        { 3, "read failed" },
    };

    public static void Generate(string outputPath, int cyls, int heads, int spt)
    {
        int total = cyls * heads * spt;
        var state = new byte[total]; // 0 = untouched
        long lastLba = -1;

        using var ctx = new HddSaverContext();
        var sectors = ctx.Sectors.ToList();

        foreach (var sec in sectors)
        {
            if (sec.Lba >= total) continue;
            if (sec.Lba > lastLba) lastLba = sec.Lba;
            if (sec.Status == SectorStatus.HeadSkip) continue; // head masked: ignored entirely
            byte lvl = sec.Status switch
            {
                SectorStatus.Ok => 1,  // green
                SectorStatus.Ecc => 2, // lighter green
                _ => 3,                // red: any other bios error code
            };
            // lower value = better read; a clean read anywhere wins over an
            // ecc-corrected one, which wins over an error
            if (state[sec.Lba] == 0 || lvl < state[sec.Lba])
                state[sec.Lba] = lvl;
        }

        int ml = 76, mt = 78;  // left margin / top offset
        int panelGap = 18, tickH = 14;

        int lastCyl = -1, lastHead = -1, lastSec = -1;
        if (lastLba >= 0)
        {
            lastCyl = (int)(lastLba / (heads * spt));
            int rem = (int)(lastLba % (heads * spt));
            lastHead = rem / spt;
            lastSec = rem % spt;
        }

        var cnt = new int[4];
        foreach (var b in state) cnt[b]++;

        // compact serialized map for the in-browser renderer: 1 hex nibble per
        // lba (state 0..3), so total * heads/2 chars. geometry + this string
        // are the entire payload the page needs to (re)draw any size.
        var sb = new System.Text.StringBuilder(total % 2 == 0 ? total / 2 : total / 2 + 1);
        for (int i = 0; i + 1 < total; i += 2) sb.Append("0123456789abcdef"[state[i] * 4 + state[i + 1]]);
        if (total % 2 == 1) sb.Append("0123456789abcdef"[state[total - 1] * 4]);
        string stateHex = sb.ToString();
        int lastLb = (int)lastLba;

        var o = new List<string>
        {
            "<!DOCTYPE html>",
            "<html><head><meta charset=\"utf-8\"><title>Tandon HDD bad-sector map</title>",
            "<style>",
            "  body{margin:0;font-family:Consolas,monospace;background:#fafafa;display:flex;flex-direction:column;height:100vh;overflow:hidden}",
            "  header{background:#ffffff;border-bottom:1px solid #cccccc;padding:8px 14px;flex:none}",
            "  h1{font-size:15px;margin:0 0 5px 0}",
            "  .legend span{margin-right:14px;font-size:11px;vertical-align:middle}",
            "  .legend i{display:inline-block;width:10px;height:10px;margin-right:5px;vertical-align:middle}",
            "  .toolbar{margin-top:6px;font-size:11px;display:flex;align-items:center;gap:8px}",
            "  .toolbar input[type=range]{width:260px}",
            "  .toolbar output{min-width:90px}",
            "  .counts{font-size:11px;margin-top:5px}",
            "  .counts b{color:#40414f}",
            "  .readout{font-size:12px;font-weight:bold;margin-top:5px;min-height:12px}",
            "  .wrap{flex:1;min-height:0;overflow:auto;padding:6px 14px 20px}",
            "  svg{cursor:crosshair}",
            "</style></head><body>",
            "<header>",
            $"  <h1>Tandon HDD bad-sector map ({sectors.Count} sectors)</h1>",
            "  <div class=\"legend\">",
        };

        foreach (var st in new[] { 1, 2, 3, 0 })
        {
            o.Add($"    <span><i style=\"background:{MapColors[st]}\"></i>{MapNames[st]}</span>");
        }
        o.Add("  </div>");
        o.Add($"  <div class=\"toolbar\"><label for=\"cz\" >Cell size:</label>" +
              "<input id=\"cz\" type=\"range\" min=\"2\" max=\"14\" step=\"1\" value=\"3\">" +
              "<output id=\"czv\"></output></div>");
        o.Add($"  <div class=\"counts\">{cnt[1]} ok | {cnt[2]} ecc corrected | {cnt[3]} read failed | {cnt[0]} missing" +
              (lastLba >= 0 ? $" <b>| ends at lba {lastLba} (head {lastHead} sec {lastSec} cyl {lastCyl})</b>" : "") + "</div>");
        o.Add("  <div class=\"readout\" id=\"readout\">LBA -</div>");
        o.Add("</header>");
        o.Add("<div class=\"wrap\"></div>");
        string scriptTpl = """
<script>
var CYL=@@CYL@@,HEADS=@@HEADS@@,SPT=@@SPT@@,ML=@@ML@@,MT=@@MT@@,GAP=@@GAP@@,TICKH=@@TICKH@@;
var STATE='@@STATE@@';
var LAST=@@LAST@@;
var COLORS=['@@C0@@','@@C1@@','@@C2@@','@@C3@@'];
var wrap=document.querySelector('.wrap');
var rt=document.getElementById('readout');
var cz=document.getElementById('cz'),czv=document.getElementById('czv');
var svg,hv;
function st(l){var v=parseInt(STATE[Math.floor(l/2)],16);return(v>>(l%2?0:2))&3;}
function render(){
  var cell=+cz.value;
  czv.textContent=cell+' px';
  var CW=cell,CH=cell;
  var panelH=SPT*CH+TICKH,step=panelH+GAP;
  var W=ML+CYL*CW+14,H=MT+HEADS*step;
  var h=[];
  h.push('<svg id="map" xmlns="http://www.w3.org/2000/svg" width="'+W+'" height="'+H+'" font-family="monospace">');
  h.push('<rect width="100%" height="100%" fill="#fafafa"/>');
  for(var hd=0;hd<HEADS;hd++){
    var y0=MT+hd*step,gy=y0+16;
    h.push('<text x="'+ML+'" y="'+(y0+10)+'" font-size="12" fill="#222">Head '+hd+'</text>');
    for(var c=0;c<=CYL;c+=100)h.push('<line x1="'+(ML+c*CW)+'" y1="'+gy+'" x2="'+(ML+c*CW)+'" y2="'+(gy+SPT*CH)+'" stroke="#dddddd"/>');
    for(var s=0;s<SPT;s+=5)h.push('<text x="'+(ML-24)+'" y="'+(gy+s*CH+9)+'" font-size="9" text-anchor="end" fill="#666">'+s+'</text>');
    for(var s2=0;s2<SPT;s2++){
      var ry=gy+s2*CH,x=0;
      while(x<CYL){
        var stx=st((x*HEADS+hd)*SPT+s2),x2=x+1;
        while(x2<CYL&&st((x2*HEADS+hd)*SPT+s2)===stx)x2++;
        h.push('<rect x="'+(ML+x*CW)+'" y="'+ry+'" width="'+((x2-x)*CW)+'" height="'+CH+'" fill="'+COLORS[stx]+'"/>');
        x=x2;
      }
    }
    for(var c2=0;c2<CYL;c2+=100)h.push('<text x="'+(ML+c2*CW)+'" y="'+(gy+SPT*CH+11)+'" font-size="9" fill="#666">'+c2+'</text>');
  }
  if(LAST>=0){
    var lc=Math.floor(LAST/(HEADS*SPT)),lh=Math.floor((LAST%(HEADS*SPT))/SPT),ls=LAST%SPT;
    h.push('<rect x="'+(ML+lc*CW)+'" y="'+(MT+lh*step+16+ls*CH)+'" width="'+CW+'" height="'+CH+'" fill="none" stroke="#cc0000" stroke-width="2"/>');
  }
  h.push('<rect id="hover" x="0" y="0" width="0" height="0" fill="none" stroke="#000" stroke-width="1" pointer-events="none"/>');
  h.push('</svg>');
  wrap.innerHTML=h.join('');
  svg=document.getElementById('map');hv=document.getElementById('hover');
}
function place(evt){
  var cell=+cz.value,CW=cell,CH=cell;
  var panelH=SPT*CH+TICKH,step=panelH+GAP;
  var r=svg.getBoundingClientRect();
  var ix=evt.clientX-r.left-ML,iy=evt.clientY-r.top-MT;
  var cyl=Math.floor(ix/CW);if(cyl<0)cyl=0;if(cyl>CYL-1)cyl=CYL-1;
  var h=Math.floor(iy/step);if(h<0)h=0;if(h>HEADS-1)h=HEADS-1;
  var sec=Math.floor((iy-h*step-16)/CH);if(sec<0)sec=0;if(sec>SPT-1)sec=SPT-1;
  var lba=(cyl*HEADS+h)*SPT+sec;
  hv.setAttribute('x',ML+cyl*CW);hv.setAttribute('y',MT+h*step+16+sec*CH);
  hv.setAttribute('width',CW);hv.setAttribute('height',CH);
  rt.textContent='LBA '+lba+' | head '+h+' | sec '+sec+' | cyl '+cyl;
}
cz.addEventListener('input',render);
render();
// delegation on the stable wrapper: survives svg rebuilds on size change
wrap.addEventListener('mousemove',function(e){if(svg&&hv)place(e);});
wrap.addEventListener('mouseleave',function(){rt.textContent='LBA -';if(hv)hv.setAttribute('width','0');});
</script>
""".
            Replace("@@CYL@@", cyls.ToString()).
            Replace("@@HEADS@@", heads.ToString()).
            Replace("@@SPT@@", spt.ToString()).
            Replace("@@ML@@", ml.ToString()).
            Replace("@@MT@@", mt.ToString()).
            Replace("@@GAP@@", panelGap.ToString()).
            Replace("@@TICKH@@", tickH.ToString()).
            Replace("@@STATE@@", stateHex).
            Replace("@@LAST@@", (lastLba < 0 ? -1 : lastLb).ToString()).
            Replace("@@C0@@", MapColors[0]).
            Replace("@@C1@@", MapColors[1]).
            Replace("@@C2@@", MapColors[2]).
            Replace("@@C3@@", MapColors[3]);
        o.Add(scriptTpl);
        o.Add("</body></html>");

        File.WriteAllText(outputPath, string.Join("\n", o));
    }
}