<?php 
for ($r=0; $r<4; $r++) {
?>
                <div id="hide<?php echo $r;?>" class="pure-u-1 pure-u-sm-1-2" style="display: none;">
                    <div class="relay">
                        <div class="relay-header">
                            <h2 id="name<?php echo $r;?>">Relay <?php echo $r+1;?></h2>
                            <h4 id="trigger<?php echo $r;?>"></h4>
                        </div>
                        <div>
                            <label class="switch">
                                <input id="relay<?php echo $r;?>" type="checkbox" name="relay<?php echo $r;?>" onclick="relayturn(this);"><span class='switch-label'></span>
                            </label>
                            <input id="auto<?php echo $r;?>" type="checkbox" name="auto<?php echo $r;?>" onclick="relayauto(this);"> <label for="auto<?php echo $r;?>">automático</label>
                        </div>
                    </div>
                </div>
<?php
}
?>