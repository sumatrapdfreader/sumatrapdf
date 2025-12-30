package org.brotli.wrapper.android;

import static junit.framework.Assert.assertEquals;

import androidx.test.core.app.ApplicationProvider;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

@RunWith(RobolectricTestRunner.class)
public final class UseJniTest {

  @Before
  public void setup() {
    JniHelper.context = ApplicationProvider.getApplicationContext();
  }

  @Test
  public void testAnswerToTheUltimateQuestionOfLifeTheUniverseAndEverything() {
    assertEquals(42, UseJni.deepThought());
  }
}
